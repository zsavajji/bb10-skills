package main

import (
	"context"
	"database/sql"
	"sync"
	"time"

	"github.com/polymorfa/hypermeow"
	"github.com/polymorfa/hypermeow/proto/waHistorySync"
	"github.com/polymorfa/hypermeow/types"
	"github.com/polymorfa/hypermeow/types/events"
	waLog "github.com/polymorfa/hypermeow/util/log"
)

const (
	batchOps  = 500
	batchTime = 50 * time.Millisecond
	// [...] app logic left out: backlogCap, muteForever
)

// Store projects WhatsApp events into chats.db (PLAN.md §4) and tells the UI which rows changed.
type Store struct {
	db   *sql.DB
	srv  *Server
	log  waLog.Logger
	ops  chan func(*storeTx)
	done chan struct{}
	quit bool // only the writer touches it
	// backlog is set from OfflineSyncPreview to OfflineSyncCompleted while the server hands over
	// what came during downtime; those chats reach the Hub only at the end, so chats read on the
	// main phone meanwhile don't alert (the user's call, 2026-11-01). Only the writer touches it.
	backlog map[string]bool
	ui      uiState
	nt      *Notifier
	// historyIn is called once a recent-chats history sync has committed: the session ends its
	// "syncing" then (step 5.5, after the Q10 test). Set before the client connects.
	historyIn func()
	// dl holds a *sync.Mutex per message being downloaded, so a tap and the live download of a
	// photo don't both write its file. ponytail: one small mutex per downloaded message, never pruned.
	dl sync.Map
	// hist takes the history-sync notifications and works them a chat at a time (history_stream.go).
	hist *historyQueue
}

func newStore(db *sql.DB, srv *Server, log waLog.Logger) *Store {
	st := &Store{db: db, srv: srv, log: log, ops: make(chan func(*storeTx), 1024), done: make(chan struct{})}
	st.nt = newNotifier(srv.dir, log)
	transLog = log
	st.hist = newHistoryQueue(st)
	srv.Handle("prefs.set", st.handlePrefs)
	go st.run()
	return st
}

// run is the one writer. It groups ops into transactions of up to 500 ops or 50 ms, so a
// history sync is a few commits and a lone message is one.
func (st *Store) run() {
	defer close(st.done)
	for op := range st.ops {
		st.batch(op)
		if st.quit {
			return
		}
	}
}

func (st *Store) batch(first func(*storeTx)) {
	tx, err := st.db.Begin()
	if err != nil {
		st.log.Warnf("begin: %v", err)
		return // ponytail: the ops of this batch are lost; chats.db is not locked for long, so it is rare
	}
	t := &storeTx{st: st, tx: tx, log: st.log, chats: map[string]bool{}, msgs: map[string]map[string]bool{}}
	t.run(first)
	timer := time.NewTimer(batchTime)
	defer timer.Stop()
fill:
	// A clear ends its batch, so its wipe runs before anything written after it (a re-link's seed).
	for n := 1; n < batchOps && !st.quit && !t.wipe; n++ {
		select {
		case op := <-st.ops:
			t.run(op)
		case <-timer.C:
			break fill
		}
	}
	if err := tx.Commit(); err != nil {
		st.log.Warnf("commit: %v", err)
		tx.Rollback()
		return
	}
	t.emit(st.srv)
	st.nt.send(t.notes)
	if t.wipe {
		st.wipe()
	}
	for _, f := range t.after {
		f()
	}
}

// afterCommit queues f to run once everything queued before it has committed.
func (st *Store) afterCommit(f func()) {
	st.ops <- func(t *storeTx) { t.after = append(t.after, f) }
}

// [...] app logic left out: wipe, which removes the old link's files once clear has committed

// handle queues an event for the writer. It only enqueues: hypermeow calls it on its receive loop.
func (st *Store) handle(cli *whatsmeow.Client, evt any) {
	if st.takeHistory(cli, evt) {
		return
	}
	switch e := evt.(type) {
	case *events.Connected:
		go st.syncGroups(cli)
		go st.syncChannels(cli)
		go st.hist.resume(cli) // the chunks a crash or a failure left on disk
		st.ops <- func(t *storeTx) {
			t.tidy(cli)
			syncSettings(context.Background(), t, cli)
			if t.stateInt("appstate_resync") == 1 {
				go st.resyncAppState(cli) // off the writer: it is network
			}
		}
		return
	// [...] app logic left out: newsletter, offline-sync backlog and view-once events
	case *events.HistorySync, *events.Message, *events.Contact, *events.LIDContact, *events.AppStateSyncComplete,
		*events.PushName, *events.GroupInfo, *events.JoinedGroup, *events.Mute, *events.Pin, *events.Archive, *events.UnarchiveChatsSetting, *events.UserStatusMute, *events.Star, *events.MarkChatAsRead,
		*events.Receipt, *events.ChatPresence, *events.Presence, *events.DeleteForMe, *events.ClearChat, *events.DeleteChat:
		st.ops <- func(t *storeTx) { st.dispatch(t, cli, evt) }
		if h, ok := e.(*events.HistorySync); ok && st.historyIn != nil {
			switch h.Data.GetSyncType() {
			case waHistorySync.HistorySync_INITIAL_BOOTSTRAP, waHistorySync.HistorySync_RECENT:
				st.afterCommit(st.historyIn)
			}
		}
	}
}

// [...] app logic left out: resyncAppState, syncGroups, joinedGroupNames

func (st *Store) dispatch(t *storeTx, cli *whatsmeow.Client, evt any) {
	ctx := context.Background()
	lids := cli.Store.LIDs
	switch e := evt.(type) {
	// [...] app logic left out: history, messages, contacts, names, receipts, presence, mute
	case *events.Pin:
		t.setPinned(chatKey(ctx, t, lids, e.JID, types.EmptyJID), e.Action.GetPinned())
		// [...] app logic left out: archive, star, read marks, deletes, clears
	}
}

// [...] app logic left out: rename, muteUntil, settingsMute, clear

// close flushes the queued writes and stops the writer.
func (st *Store) close() {
	st.ops <- func(*storeTx) { st.quit = true }
	<-st.done
	st.nt.close()
}

// storeTx is one open transaction and the rows it touched.
type storeTx struct {
	st    *Store // nil in tests of single writes
	notes map[string]note
	tx    *sql.Tx
	log   waLog.Logger
	chats map[string]bool
	msgs  map[string]map[string]bool // chat -> ids
	wipe  bool                       // clear ran: wipe after the commit
	after []func()                   // afterCommit's
	// A story was added, changed or dropped (stories.go): emit "stories" after the commit.
	stories bool
	// A view of one of our stories was recorded: emit "storyviews" after the commit.
	views bool
	stmts map[string]*sql.Stmt // exec's, by their SQL
}

// run runs one op; a failing or panicking op doesn't stop the batch.
func (t *storeTx) run(op func(*storeTx)) {
	defer func() {
		if r := recover(); r != nil {
			t.log.Warnf("store op panicked: %v", r)
		}
	}()
	op(t)
}

// exec runs a statement and logs a failure. It reports how many rows changed. A statement is
// prepared once per transaction, which closes it when it ends: a history sync runs the same insert
// thousands of times in one, and preparing it each time was two thirds of the write (on the PC,
// 2026-11-13: an insert and its setLast went from 9.4 to 2.8 µs).
func (t *storeTx) exec(q string, args ...any) int64 {
	s := t.stmts[q]
	if s == nil {
		var err error
		if s, err = t.tx.Prepare(q); err != nil {
			t.log.Warnf("store: %v", err)
			return 0
		}
		if t.stmts == nil {
			t.stmts = map[string]*sql.Stmt{}
		}
		t.stmts[q] = s
	}
	res, err := s.Exec(args...)
	if err != nil {
		t.log.Warnf("store: %v", err)
		return 0
	}
	n, _ := res.RowsAffected()
	return n
}

// touch runs an UPDATE of one chat and marks it changed when a row changed.
func (t *storeTx) touch(jid, q string, args ...any) bool {
	if t.exec(q, append(args, jid)...) > 0 {
		t.chats[jid] = true
		return true
	}
	return false
}

// [...] app logic left out: the other row writers (state, names, last message, unread, mute)

func (t *storeTx) setPinned(jid string, v bool) {
	t.touch(jid, "UPDATE chats SET pinned = ?1 WHERE pinned != ?1 AND jid = ?2", v)
}

// [...] app logic left out: setArchived, insertMessage, edits, emit (tells the UI what the batch changed), syncSettings
