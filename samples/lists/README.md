# samples/lists

C++-drawn list rows. Not built in this repo; read as reference. Docs: [13-design-and-screens.md](../../docs/13-design-and-screens.md).

| File | From | Kind | Shows |
|---|---|---|---|
| ChatRows.h, ChatRows.cpp | tengram `5a65cdf` app/src | verbatim | Chat list rows drawn in C++ |
| MessageCards.h, MessageCards.cpp | tengram `5a65cdf` app/src | verbatim | Message cards drawn in C++ |
| MessageCard.h, MessageCard.cpp | tengram `5a65cdf` app/src | verbatim | One message card drawn in C++ |
| AlbumBubble.h, AlbumBubble.cpp | tengram `5a65cdf` app/src | verbatim | One square of an album, with a play mark on video |
| MediaBubble.h, MediaBubble.cpp | tengram `5a65cdf` app/src | verbatim | Photo, video and map bubble in C++ |
| ChatListModel.h, ChatListModel.cpp | tengram `5a65cdf` app/src | verbatim | Chat list model: read-only chats.db, pinned first, date headers |
| MessageModel.h | tengram `5a65cdf` app/src | verbatim | Message model for one chat, first page sized by on-screen cost |
| ChatPage.qml | tengram `5a65cdf` app/assets | verbatim | One chat page: title bar and the messages as cards |
| ChatList.qml | tengram `5a65cdf` app/assets | verbatim | Rows of a chat list; the rows are C++ |
| queue.js | bbmusic `1d80813` app/assets | verbatim | Queue page arithmetic kept out of QML so node can test it |
