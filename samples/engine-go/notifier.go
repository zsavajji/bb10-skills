package main

import (
	"database/sql"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"time"

	waLog "github.com/polymorfa/hypermeow/util/log"
)

// What the notifier calls on the Hub; tests swap them for a recorder.
var (
	hubOpenFn    = udsOpen
	hubAccountFn = udsAccount
	hubItemFn    = udsItem
	hubRemoveFn  = udsRemove
)

// [...] app logic left out: uiState, what the UI last said about the chat on screen (seen, shown, openChat)

// note is what a committed transaction wants done to one chat's Hub item. add: a live incoming
// message, so the item may be created; otherwise only an existing item changes. alert: the Hub
// alerts (sound, LED, the instant preview). gone: the chat is deleted or archived, so its item
// goes and nothing else is done. ts is unix seconds.
type note struct {
	add, alert, gone bool
	title, preview   string
	unread           int
	ts               int64
}

// hubItem queues the chat's item as the chat is now: its name, last message and unread count.
// alert asks for the Hub's alert, which stays only for a chat with unread messages that is
// neither muted nor on screen. A muted chat's item shows read unless marked unread by hand. An archived chat has no item at all (the user's, 2026-11-06), so this
// takes away any it has. Call it after the row has its new values.
func (t *storeTx) hubItem(chat string, add, alert bool) {
	var name, preview sql.NullString
	var last sql.NullInt64
	var unread int
	var muted int64
	var archived bool
	err := t.tx.QueryRow("SELECT name, last_preview, unread, muted_until, archived, last_ts FROM chats WHERE jid = ?", chat).
		Scan(&name, &preview, &unread, &muted, &archived, &last)
	if err != nil {
		return
	}
	if archived {
		t.note(chat, note{gone: true})
		return
	}
	if muted > time.Now().Unix() && unread > 0 {
		unread = 0 // a muted chat's item shows read, so no LED and no Hub splat (the user's, 2026-11-16)
	}
	alert = alert && unread > 0 && !t.st.ui.seen(chat)
	title := name.String
	if title == "" {
		title, _, _ = strings.Cut(chat, "@")
	}
	if title == "" {
		title = "Unknown"
	}
	ts := last.Int64
	if ts == 0 {
		ts = time.Now().Unix() // a chat with no message yet
	}
	t.note(chat, note{add: add, alert: alert, title: title, preview: preview.String, unread: unread, ts: ts})
}

// notifyIncoming queues the item of a chat that just got a live incoming message; a muted or
// on-screen chat gets it without the alert. During the offline backlog the chat waits for
// endBacklog. Call it after addUnread and setLast.
func (t *storeTx) notifyIncoming(chat string) {
	// Channels never alert, and have no Hub item.
	if isChannel(chat) {
		return
	}
	if t.st.backlog != nil {
		t.st.backlog[chat] = true
		return
	}
	t.hubItem(chat, true, true)
}

// [...] app logic left out: endBacklog, note (merges a chat's notes in a batch), hubState (hub.json)

// hubActionSet is what an account is added with: 1 was Mark read/unread, 2 adds Delete, 3 the
// white icon hub_account.png. The Hub fixes them when the account is added (it refuses an account
// update, uds 501 on the Q10), so a newer set means adding it again.
const hubActionSet = 3

// Notifier keeps the Hub account and its items from one goroutine, so UDS calls never run
// inside the store's writer and always on the same thread.
type Notifier struct {
	in      chan func()
	done    chan struct{}
	log     waLog.Logger
	path    string // prefs.json
	state   string // hub.json
	assets  string // our public assets, where the Hub looks for the icons
	open    func(assets string) (fresh bool, err error)
	account func(add bool) error
	item    func(it hubItem, add bool) error
	remove  func(id string) error

	mu     sync.Mutex
	prefs  prefs
	closed bool

	// Only the run goroutine touches these.
	ready   bool            // registered with the Hub
	acct    bool            // our account is in the Hub
	actions int             // the hubActionSet it was added with; 0 from an older build
	items   map[string]bool // chats with an item in the Hub
}

func newNotifier(dir string, log waLog.Logger) *Notifier {
	n := &Notifier{in: make(chan func(), 64), done: make(chan struct{}), log: log,
		path: filepath.Join(dir, "prefs.json"), state: filepath.Join(dir, "hub.json"),
		// dir is /accounts/1000/appdata/<pkg>/data on the phone, and the Hub reads the public
		// assets from /apps/<pkg>/public/.
		assets: "/apps/" + filepath.Base(filepath.Dir(dir)) + "/public/",
		open:   hubOpenFn, account: hubAccountFn, item: hubItemFn, remove: hubRemoveFn, items: map[string]bool{}}
	// [...] app logic left out: reading prefs.json and hub.json
	go n.run()
	return n
}

// run does the queued work. BPS and UDS belong to a thread, so it keeps this one.
func (n *Notifier) run() {
	runtime.LockOSThread()
	defer close(n.done)
	for f := range n.in {
		f()
	}
}

// [...] app logic left out: opened (registers with the Hub), save, ensureAccount, dropAccount

// apply brings one chat's item to what the note says.
func (n *Notifier) apply(chat string, nt note) {
	if nt.gone {
		n.forget(chat)
		return
	}
	if !n.opened() {
		return
	}
	known := n.items[chat]
	if !known && !nt.add {
		return
	}
	n.ensureAccount()
	if !n.acct {
		return
	}
	known = n.items[chat] // a re-added account holds none
	if !known && !nt.add {
		return
	}
	body := nt.preview
	if !n.notifyPreview() {
		body = ""
		if nt.unread != 0 {
			body = word("New message")
		}
	}
	unread := nt.unread
	if unread < 0 {
		unread = 1 // marked unread by hand
	}
	it := hubItem{hubItemID(chat), nt.title, body, unread, nt.ts * 1000, nt.alert}
	// The Hub may have lost the item, or never had one we remember: if the call is refused,
	// the other one is tried.
	add := !known
	err := n.item(it, add)
	if err != nil && (!known || nt.add) {
		add = !add
		err = n.item(it, add)
	}
	verb := "update"
	if add {
		verb = "add"
	}
	if err != nil {
		n.log.Warnf("hub: %s %s: %v", verb, chat, err)
		return
	}
	n.log.Infof("hub: %s %s alert=%v", verb, chat, nt.alert)
	if !known {
		n.items[chat] = true
		n.save()
	}
}

// [...] app logic left out: forget, removeItem

// queue puts f on the notifier's thread. ponytail: a full queue (64 behind a stuck UDS) drops
// the work rather than block the writer; a dropped unlink leaves the account until the next one.
func (n *Notifier) queue(f func(), what string) {
	n.mu.Lock()
	defer n.mu.Unlock()
	if n.closed {
		return
	}
	select {
	case n.in <- f:
	default:
		n.log.Warnf("notifier is behind, dropping %s", what)
	}
}

// send hands a committed batch's notes over.
func (n *Notifier) send(notes map[string]note) {
	if len(notes) == 0 {
		return
	}
	n.queue(func() {
		for chat, nt := range notes {
			n.apply(chat, nt)
		}
	}, "the notes")
}

// [...] app logic left out: link, unlink, close, writeAtomic
