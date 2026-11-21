package main

import (
	"encoding/json"
	"fmt"
	"sync"
	"time"

	waLog "github.com/polymorfa/hypermeow/util/log"
)

// Sim is the simulated account of step 1.12: link.* and a made-up account's chats, with nothing
// sent to WhatsApp, so the UI's flows run without linking the real number again (steps.md, "Real
// links are few"). scripts/sim.py drives it with sim.* requests: the phone accepting the device,
// a message arriving, the device removed on the phone. It also answers the chat requests from
// made-up data (step 3.0).
type Sim struct {
	srv   *Server
	store *Store
	log   waLog.Logger

	mu   sync.Mutex
	link string // as Session's
	qr   string
	code string
	run  int // counts the QR runs, so a stale one stops
	rec  voiceRec
	// syncing as Session's: from sim.link until the seed has committed.
	syncing bool
	blocked map[string]bool // contact.block's flags by chat; nil until the first
	prof    simProfile      // profile.get's name and about (sim_profile.go)
}

// simCodes are how long each made-up QR code lasts, as WhatsApp's: 60 s, then five of 20 s,
// then the run is over. simDelay stands for a round trip to WhatsApp. Tests shorten both.
var (
	simCodes = []time.Duration{60 * time.Second, 20 * time.Second, 20 * time.Second,
		20 * time.Second, 20 * time.Second, 20 * time.Second}
	simDelay = time.Second
	// simTicks are when a sent message turns sent, delivered and read, from the send.
	simTicks  = [3]time.Duration{500 * time.Millisecond, 1500 * time.Millisecond, 3 * time.Second}
	simTyping = 4 * time.Second
)

const simMe = "390000000000@s.whatsapp.net"

func newSim(srv *Server, st *Store, log waLog.Logger) *Sim {
	s := &Sim{srv: srv, store: st, log: log, link: linkUnlinked}
	// The account is linked while its chats are in: sim.link writes them, and sim.removed and
	// link.logout empty chats.db. So an engine restart keeps the state, with no file to write.
	var n int
	st.db.QueryRow("SELECT count(*) FROM chats").Scan(&n)
	if n > 0 {
		s.link = linkReady
	}
	srv.Handle("link.qr", s.handleQR)
	srv.Handle("link.code", s.handleCode)
	srv.Handle("link.logout", s.handleRemoved)
	srv.Handle("sim.link", s.handleLink)
	srv.Handle("sim.msg", s.handleMsg)
	srv.Handle("sim.removed", s.handleRemoved)
	srv.Handle("sim.typing", s.handleTyping)
	// [...] app logic left out: the voice, media, reaction, chat, group and contact requests, answered from made-up data
	return s
}

func (s *Sim) hello() any {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.body()
}

// body is the link body of docs/protocol.md.
func (s *Sim) body() map[string]any {
	b := map[string]any{"state": s.link, "conn": "offline", "memMB": memMB}
	if s.syncing {
		b["syncing"] = true
	}
	switch s.link {
	case linkQR:
		b["qr"] = s.qr
	case linkCode:
		b["code"] = s.code
	case linkReady:
		b["conn"], b["me"], b["name"] = "ready", simMe, "Sim"
	}
	return b
}

// setLink changes the link state and tells the UI. Call it with s.mu held.
func (s *Sim) setLink(state string) {
	s.link = state
	s.srv.Emit("link", s.body())
}

// [...] app logic left out: startQR (the made-up QR codes), handleQR, handleCode

// handleLink is the phone accepting the device. As with a real link: linking, then ready after
// the reconnect, then the history.
func (s *Sim) handleLink(json.RawMessage) (any, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.link != linkQR && s.link != linkCode {
		return nil, &Error{"bad_state", "nothing to accept: the linking page shows no code"}
	}
	s.run++ // ends the codes
	s.qr, s.code = "", ""
	s.store.clear()
	s.syncing = true
	s.setLink(linkLinking)
	delay := simDelay
	go func() {
		time.Sleep(delay)
		s.mu.Lock()
		defer s.mu.Unlock()
		if s.link == linkLinking {
			s.setLink(linkReady)
			s.seed()
			s.store.afterCommit(func() { go s.endSync() }) // off the writer: endSync takes s.mu
		}
	}()
	return nil, nil
}

// handleMsg is an incoming message in chat n of simChats, now.
func (s *Sim) handleMsg(req json.RawMessage) (any, error) {
	var r struct {
		N    int
		Text string
	}
	json.Unmarshal(req, &r)
	if err := s.ready(); err != nil {
		return nil, err
	}
	if r.N < 0 || r.N >= len(simChats) {
		return nil, &Error{"bad_request", fmt.Sprintf("n is 0 to %d", len(simChats)-1)}
	}
	c, now := simChats[r.N], time.Now()
	if r.Text == "" {
		r.Text = "A new message at " + now.Format("15:04:05")
	}
	s.deliver(c, now, msgRow{Kind: msgText, Text: r.Text})
	return nil, nil
}

// deliver puts an incoming message into chat c, unread, and returns its id.
func (s *Sim) deliver(c simChat, now time.Time, m msgRow) string {
	m.Chat, m.ID, m.TS, m.State, m.SenderName = c.jid, simID(now), now.Unix(), stateRead, c.sender
	if c.sender != "" {
		m.Sender = simMember(c.sender)
	}
	s.store.ops <- func(t *storeTx) {
		if t.insertMessage(m) {
			t.setLast(c.jid, m.TS, chatPreview(m), false, m.State)
			t.addUnread(c.jid, 1)
			t.notifyIncoming(c.jid)
		}
	}
	return m.ID
}

// [...] app logic left out: handleRemoved, the device removed on the phone

// endSync ends sim.link's "syncing" and tells the UI, then lets the Hub show the account.
func (s *Sim) endSync() {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.syncing {
		s.syncing = false
		s.setLink(s.link)
		s.store.nt.link() // the new link's chats are in
	}
}

// seed writes simChats, each with its last message, in one transaction.
func (s *Sim) seed() {
	now := time.Now()
	s.store.ops <- func(t *storeTx) {
		for i, c := range simChats {
			for _, h := range simHistory(i, now) {
				s.seedMedia(&h)
				t.insertMessage(h)
			}
			// [...] app logic left out: the rest of the chat's row, from simChats: last message, name, flags
		}
		seedReactions(t, now)
	}
}

// [...] app logic left out: ready, chat, simIndex, and the handlers of the other simulated behaviours (send with its ticks, delete, edit, forward, read, open, typing), each changing the same rows as the real one and sending nothing
