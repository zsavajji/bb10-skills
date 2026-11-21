package main

import (
	"context"
	"errors"
	"sync"
	"sync/atomic"
	"time"

	"github.com/polymorfa/hypermeow"
	"github.com/polymorfa/hypermeow/appstate"
	"github.com/polymorfa/hypermeow/store"
	"github.com/polymorfa/hypermeow/store/sqlstore"
	"github.com/polymorfa/hypermeow/types/events"
	waLog "github.com/polymorfa/hypermeow/util/log"
	"github.com/purpshell/meowcaller"
)

// Link states (PLAN.md §5). "syncing" is left to the store, which knows when history is in.
const (
	linkUnlinked = "unlinked"
	linkQR       = "qr"
	linkCode     = "code"
	linkLinking  = "linking"
	linkReady    = "ready"
)

// Session owns the hypermeow client and the link state machine. When the device is removed,
// by the phone or by link.logout, hypermeow deletes it from session.db, so the next link
// starts with a fresh device and a fresh client.
type Session struct {
	container *sqlstore.Container
	srv       *Server
	store     *Store
	log       waLog.Logger

	mu     sync.Mutex
	cli    *whatsmeow.Client
	link   string
	conn   string        // offline, connecting, ready
	qr     string        // the current QR code, in state qr
	code   string        // the pairing code, in state code
	stopQR func()        // ends the running QR channel
	qrSeen chan struct{} // closed when the QR channel's first code arrives
	rec    voiceRec      // the voice note being recorded
	// syncing is true from a new link until its recent chats are in chats.db, or syncCap: the
	// UI waits on the linking page meanwhile (the user's, 2026-11-01).
	syncing bool
	photos  *photoQueue                       // the chats' pictures (photos.go)
	calls   *Calls                            // the live call (calls.go)
	mc      atomic.Pointer[meowcaller.Client] // meowcaller on the current client
	// cancels holds the cancel of each media.get in flight, chat + "/" + id -> *context.CancelFunc, for media.cancel.
	cancels sync.Map
}

// syncCap ends "syncing" if the phone sends no recent-chats history: an account with no chats.
// ponytail: a guess, untested until the real link.
var syncCap = 90 * time.Second

func newSession(container *sqlstore.Container, srv *Server, store *Store, log waLog.Logger) (*Session, error) {
	device, err := container.GetFirstDevice(context.Background())
	if err != nil {
		return nil, err
	}
	s := &Session{container: container, srv: srv, store: store, log: log, link: linkUnlinked, conn: "offline"}
	s.initCalls()
	s.photos = newPhotoQueue(store, func() photoFetch { return clientFetch(s.client()) })
	store.historyIn = func() { go s.endSync() } // off the writer: endSync takes s.mu
	s.cli = s.newClient(device)
	if s.cli.Store.ID != nil {
		s.link, s.conn = linkReady, "connecting"
	}
	srv.Handle("link.qr", s.handleQR)
	srv.Handle("link.code", s.handleCode)
	srv.Handle("link.logout", s.handleLogout)
	// [...] app logic left out: handlers for send, presence, groups, invites, channels, contacts, profile, media and voice
	if s.cli.Store.ID != nil {
		// Not waited for: the UI gets its answers at once, whatever the network is doing.
		cli := s.cli
		go connectUntil(cli.Connect, func() bool { return cli != s.client() }, time.Minute, s.log)
	}
	return s, nil
}

// reconnectCap is the failures at which hypermeow's wait, 2 s for each, stops growing: 2 minutes.
// Left alone, three days offline means 17 minutes before it looks again.
const reconnectCap = 60

// configureReconnect has a first connect with no network retry as a later one does. hypermeow's
// default returns the error, which a reboot with no network would meet every time.
func configureReconnect(cli *whatsmeow.Client) {
	cli.InitialAutoReconnect = true
	cli.AutoReconnectHook = func(error) bool {
		cli.AutoReconnectErrors = min(cli.AutoReconnectErrors, reconnectCap)
		return true
	}
}

// connectUntil runs the first connect. A network failure doesn't reach here (configureReconnect),
// but anything else would leave the engine connecting for ever, so it tries again. stale says a logout replaced this client.
func connectUntil(connect func() error, stale func() bool, pause time.Duration, log waLog.Logger) {
	for !stale() {
		err := connect()
		if err == nil || errors.Is(err, whatsmeow.ErrAlreadyConnected) {
			return
		}
		log.Errorf("connect: %v", err)
		time.Sleep(pause)
	}
}

func (s *Session) newClient(device *store.Device) *whatsmeow.Client {
	cli := whatsmeow.NewClient(device, s.log.Sub("client"))
	// hypermeow would acknowledge a history chunk before downloading it and then read it whole.
	// It only dispatches the notification now; the Store works it and sends the receipt itself.
	cli.ManualHistorySyncDownload = true
	cli.DisableManualHistorySyncReceipt = true
	// A full app-state sync (a link, or a gap in the versions) emits nothing but these, so a star, a muted
	// story or "Keep chats archived" set on the phone before the link reaches us; the rest of the full
	// sync stays silent, since EmitAppStateEventsOnFullSync would replay mark-read, contacts and deletes.
	cli.EmitFullSyncIndexes = map[string]bool{appstate.IndexStar: true, appstate.IndexUserStatusMute: true, appstate.IndexSettingUnarchiveChats: true}
	configureReconnect(cli)
	s.callsOn(cli) // before it connects: meowcaller listens to the call stanzas from the start
	cli.AddEventHandler(func(evt any) { s.onEvent(cli, evt) })
	cli.AddEventHandler(func(evt any) { s.store.handle(cli, evt) })
	return cli
}

// client returns the current client; a logout replaces it, so take it once per request.
func (s *Session) client() *whatsmeow.Client {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.cli
}

// [...] app logic left out: openChat helper

// hello is the link part of the hello reply.
func (s *Session) hello() any {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.linkBody()
}

func (s *Session) linkBody() map[string]any {
	b := map[string]any{"state": s.link, "conn": s.conn, "memMB": memMB}
	if s.syncing {
		b["syncing"] = true
	}
	switch {
	case s.link == linkQR:
		b["qr"] = s.qr
	case s.link == linkCode:
		b["code"] = s.code
	case s.cli.Store.ID != nil:
		b["me"] = s.cli.Store.ID.ToNonAD().String()
		b["name"] = s.cli.Store.PushName
	}
	return b
}

// setLink changes the link state and tells the UI. Call it with s.mu held.
func (s *Session) setLink(state string) {
	s.link = state
	s.srv.Emit("link", s.linkBody())
}

// endSync ends a new link's "syncing" and tells the UI, then lets the Hub show the account.
func (s *Session) endSync() {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.syncing {
		s.syncing = false
		s.setLink(s.link)
		s.store.nt.link() // the new link's chats are in
		s.photos.round()  // and need their pictures
	}
}

// [...] app logic left out: close, QR and pairing-code login, logout, reset

func (s *Session) onEvent(cli *whatsmeow.Client, evt any) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if cli != s.cli {
		return // an event from a client that reset() already replaced
	}
	switch e := evt.(type) {
	case *events.PairSuccess:
		// A new link starts a new history: nothing from an older link, or from the simulated
		// account (sim.go), stays in chats.db.
		s.store.clear()
		s.syncing = true
		s.setLink(linkLinking)
		time.AfterFunc(syncCap, s.endSync)
	case *events.Connected:
		s.conn = "ready"
		if cli.Store.ID != nil {
			if s.stopQR != nil {
				s.stopQR()
				s.stopQR = nil
			}
			s.setLink(linkReady)
			// [...] app logic left out: presence sent once connected
		}
		s.srv.Emit("conn", map[string]any{"state": s.conn})
		s.photos.connected()
	// [...] app logic left out: picture changes (case *events.Picture)
	case *events.Disconnected:
		// [...] app logic left out: the grace period before a dropped connection ends a call
		s.conn = "connecting" // hypermeow reconnects by itself
		s.srv.Emit("conn", map[string]any{"state": s.conn})
	case *events.StreamReplaced:
		s.calls.drop()
		s.conn = "offline" // another engine took this session over
		s.srv.Emit("conn", map[string]any{"state": s.conn, "reason": "replaced"})
	case *events.LoggedOut:
		s.log.Warnf("logged out: %v", e.Reason)
		// Off this goroutine: reset disconnects the very client delivering the event.
		go func() {
			s.mu.Lock()
			defer s.mu.Unlock()
			if cli == s.cli {
				s.reset()
			}
		}()
	}
}
