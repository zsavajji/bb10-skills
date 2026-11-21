package main

import (
	"context"
	"encoding/json"
	"strings"
	"time"

	"github.com/polymorfa/hypermeow"
	"github.com/polymorfa/hypermeow/proto/waE2E"
	"github.com/polymorfa/hypermeow/types"
)

const sendTimeout = 60 * time.Second

// linked returns the client of a linked device and the chat's JID, or the error to reply with.
func (s *Session) linked(chat string) (*whatsmeow.Client, types.JID, error) {
	cli := s.client()
	if cli.Store.ID == nil {
		return nil, types.EmptyJID, &Error{"unlinked", "link this device first"}
	}
	jid, err := types.ParseJID(chat)
	if err != nil || jid.IsEmpty() {
		return nil, types.EmptyJID, &Error{"bad_chat", "not a chat key: " + chat}
	}
	return cli, jid, nil
}

func registerSend(srv *Server, s *Session) {
	srv.Handle("msg.send", s.handleSend)
	// [...] app logic left out: the other msg, poll, chat and contact handlers
}

func (s *Session) handleSend(req json.RawMessage) (any, error) {
	var r struct {
		Chat, Text, Quote string
		Mentions          []mention
	}
	if err := json.Unmarshal(req, &r); err != nil || strings.TrimSpace(r.Text) == "" {
		return nil, &Error{"bad_request", "msg.send needs chat and non-empty text"}
	}
	cli, jid, err := s.linked(r.Chat)
	if err != nil {
		return nil, err
	}
	id, now := cli.GenerateMessageID(), time.Now().Unix()

	// Our row keeps the text as typed ("@Anna Lee") and the mentions, so its bubble links them as a
	// received one does; the wire message has "@<number>".
	wire, jids := mentionWire(r.Text, r.Mentions)
	extra := ""
	if len(jids) > 0 {
		extra = withMentions("", r.Mentions)
	}
	q, created := s.store.queueOutgoing(jid, r.Chat, id, r.Text, r.Quote, extra, now)
	s.nameNew(cli, jid, r.Chat, created)
	msg := &waE2E.Message{Conversation: &r.Text}
	if q.ID != "" || len(jids) > 0 {
		ci := &waE2E.ContextInfo{}
		if q.ID != "" {
			ci = quoteInfo(cli, r.Chat, q)
		}
		ci.MentionedJID = jids
		msg = &waE2E.Message{ExtendedTextMessage: &waE2E.ExtendedTextMessage{Text: &wire, ContextInfo: ci}}
	}

	ctx, cancel := context.WithTimeout(context.Background(), sendTimeout)
	defer cancel()
	resp, err := cli.SendMessage(ctx, jid, msg, whatsmeow.SendRequestExtra{ID: id})
	if err != nil {
		s.store.ops <- func(t *storeTx) { t.sentAs(r.Chat, id, 0, stateFailed) }
		return nil, err
	}
	// The server's clock, not the phone's: it drifts.
	s.store.ops <- func(t *storeTx) { t.sentAs(r.Chat, id, resp.Timestamp.Unix(), stateSent) }
	return map[string]string{"id": id}, nil
}

// [...] app logic left out: nameNew, quoteInfo, delete, edit and the app-state patch for delete-for-me

// queueOutgoing queues our text; extra is the row's (its mentions), "" for none.
func (st *Store) queueOutgoing(jid types.JID, chat, id, text, quote, extra string, now int64) (msgRow, bool) {
	return st.queueRow(jid, chat, quote, msgRow{Chat: chat, ID: id, TS: now, Out: true, State: stateSending, Kind: msgText, Text: text, Extra: extra})
}

// queueRow puts our message in as sending, so the UI shows the bubble while the send is in
// flight, and returns the message it quotes (empty when there is none) and whether the chat
// was new.
func (st *Store) queueRow(jid types.JID, chat, quote string, row msgRow) (msgRow, bool) {
	quoted := make(chan msgRow, 1)
	made := make(chan bool, 1)
	st.ops <- func(t *storeTx) {
		made <- !t.hasChat(chat)
		var q msgRow
		if quote != "" {
			if !isCallID(quote) { // a call row can't be quoted: WhatsApp doesn't know it
				q, _ = t.messageByID(chat, quote)
			}
		}
		quoted <- q
		if q.ID != "" {
			row.QuoteID, row.QuotePreview = q.ID, preview(q.Kind, q.Text, q.Duration)
		}
		t.ensureChat(chat, chatKind(jid))
		t.insertMessage(row)
		t.setLast(chat, row.TS, chatPreview(row), true, stateSending)
	}
	return <-quoted, <-made
}

// [...] app logic left out: receipt, which moves the ticks of our messages
