#include "MessageCard.h"

#include <bb/cascades/Container>
#include <bb/cascades/DockLayout>
#include <bb/cascades/FontStyle>
#include <bb/cascades/HorizontalAlignment>
#include <bb/cascades/ImageView>
#include <bb/cascades/Label>
#include <bb/cascades/ListView>
#include <bb/cascades/ScalingMethod>
#include <bb/cascades/SystemDefaults>
#include <bb/cascades/TextFormat>
#include <bb/cascades/UIConfig>
#include <bb/cascades/VerticalAlignment>

#include "AlbumBubble.h"
#include "CardStyle.h"
#include "MediaBubble.h"
#include "VoiceBubble.h"

using namespace bb::cascades;

MessageCard::MessageCard(ListView *list, bool rich, bool picture)
    : m_list(list)
    , m_ui(list->ui())
    , m_rich(rich)
    , m_pressed(false)
    , m_timePlace(-1)
    , m_timeTop(-1)
    , m_colMax(-1)
    , m_tick(-1)
    , m_downX(0)
    , m_downY(0)
    , m_swipe(0)
    , m_styled(false)
    , m_restyledOut(false)
    , m_round(false)
    , m_scale(0)
    // [...] app logic left out: the other parts, 0 until a message first needs them (sender, tile, quote, bubbles, chips), the translated marks, the menu's actions
{
    // The card's own node and its root: a list item is as wide as its content unless it fills.
    // Theirs 1.5 du from the left edge and ours from the right.
    setHorizontalAlignment(HorizontalAlignment::Fill);
    m_root = new Container();
    m_root->setHorizontalAlignment(HorizontalAlignment::Fill);
    m_root->setLeftPadding(m_ui->du(1.5));
    m_root->setRightPadding(m_ui->du(1.5));

    // The card: the text column, and the time and ticks where placeTime() puts them, never on a row
    // of their own, so a one-line message is one line tall (Q10, 2026-10-31: a row of its own made
    // the cards too tall). The labels in the card set their margins: unset, Cascades puts about 20 px
    // between them.
    m_card = new Container();
    m_card->setHorizontalAlignment(HorizontalAlignment::Fill);
    m_card->setLayout(Card::across());

    // As wide as what it holds, up to the width fillFoot() allows it: with a quota it took the
    // whole row, so a one-letter message was as wide as the screen. The picture card is the
    // picture's width, so there the column takes what the time leaves.
    m_col = new Container();
    if (picture)
        Card::quota(m_col, 1);

    // The text, and over its last line's end the time: fillColumn() ends the text with blanks as
    // wide as the time, so the line stops short of it, or the blanks wrap and the time has a line
    // to itself, as in Telegram (the user, 2026-11-14: beside the first line, the time kept a
    // column that was empty below it). Filled, so the time is at the column's right edge.
    m_words = new Container();
    m_words->setHorizontalAlignment(HorizontalAlignment::Fill);
    m_words->setTopMargin(0);
    m_words->setLayout(new DockLayout());
    m_text = new Label();
    m_text->setMultiline(true);
    // [...] app logic left out: the handler that opens a mention's link
    m_words->add(m_text);
    m_col->add(m_words);

    m_timeBox = new Container();
    m_timeBox->setLayout(Card::across());

    // One label for "edited" and the time: a separate one never showed on the Q10 (2026-10-31). A
    // percentage of its base, SmallText: the same at Normal, and growing with the text size as the body does.
    m_time = new Label();
    m_time->setVerticalAlignment(VerticalAlignment::Center);
    m_time->setTextFormat(TextFormat::Plain);
    m_time->textStyle()->setBase(SystemDefaults::TextStyles::smallText());
    m_time->textStyle()->setFontSize(FontSize::PercentageValue);
    m_ticks = new ImageView();
    m_ticks->setVerticalAlignment(VerticalAlignment::Center);
    // Sized by fillFoot() with the time's text; 1 du from it, not the 20 px Cascades puts there.
    m_ticks->setScalingMethod(ScalingMethod::AspectFit);
    m_ticks->setLeftMargin(m_ui->du(1));
    // A self-destructing message's clock (m_timer) is made by makeTimer() when one first needs it.
    m_timeBox->add(m_time);
    m_timeBox->add(m_ticks);

    m_card->add(m_col);

    // [...] app logic left out: the foot, the card's colour and frame, the row with its spacers, setRoot, the slide's layout handler, the long-press menu

    // The search match is tinted: the page sets foundId.
    if (!connect(list, SIGNAL(foundIdChanged()), this, SLOT(paintBackground())))
        qWarning("cards: the list has no foundIdChanged");
    // The picture's corners are masks in the page's colour (paintCorners): a theme or Dark background change paints them again.
    if (!connect(list, SIGNAL(pageColorChanged()), this, SLOT(paintBackground())))
        qWarning("cards: the list has no pageColorChanged");
}

// [...] app logic left out: makeTail, the foot's triangle

void MessageCard::show(const QVariantMap &msg)
{
    m_msg = msg;
    reportSlide(m_list->property("slideId").toString()); // a reused card that won't be laid out again (reportSlide)
    const bool out = flag("out");
    const bool deleted = flag("deleted");

    // The sender's tile: a group's incoming message, which has a sender id. The name line only shows on the oldest of
    // a person's messages in a row, so a card that never shows it never makes it.
    const bool tiled = !out && !str("senderId").isEmpty();

    // The parts this message needs and the card hasn't made yet.
    if (named() && !m_sender)
        makeSender();
    if (tiled && !m_avatar)
        makeTile();
    if (m_msg.value("forwarded").toInt() > 0 && !m_fwd)
        makeForwarded();
    if (!str("quote").isEmpty() && !m_quote)
        makeQuote();
    if (m_rich && !str("label").isEmpty() && !m_label)
        makeLabel();
    if (m_rich && !str("invite").isEmpty() && !deleted && !m_join)
        makeJoin();
    if (m_rich)
        makeBubbles();
    if (out && m_msg.value("state").toInt() == 4 && !m_bang)
        makeBang();
    const int chips = m_msg.value("reactions").toList().size();
    if (chips > 0 && !m_chip)
        makeChip();
    if (chips > m_chipBoxes.size())
        growChips(chips);
    restyle(out);

    // Theirs at the left and ours at the right. A hidden spacer takes no part in the layout.
    m_gapL->setVisible(out);
    m_gapR->setVisible(!out);
    fillTile(tiled);
    fillColumn();
    if (m_rich)
        fillBubbles();
    fillFoot();
    fillMenu();
    paintBackground();
}

// Pressed: the page's pressColor. A search match: the primary colour at 20 % over the page, so it reads
// in both themes (25 % until the user found it strong, 2026-11-17). Else Telegram's bubble colour, ours or theirs
// (the user's ask, 2026-11-18); a sticker has none, it sits on the page (user). An empty foundId matches nothing.
void MessageCard::paintBackground()
{
    const QString found = Card::foundId(m_list);
    const bool match = !found.isEmpty() && found == str("id");
    const Color c = m_pressed ? Card::color(Card::pressColor(m_list))
                  : match ? Card::color("#3324a1de")
                  : flag("out") ? Card::color(Card::outColor(m_list)) : Card::color(Card::inColor(m_list));
    m_bg->setVisible(m_pressed || match || str("media") != "sticker");
    m_bg->setFilterColor(c);
    m_tailL->setFilterColor(c);
    m_tailR->setFilterColor(c);
    if (m_media)
        paintCorners(c);
}

// [...] app logic left out: paintCorners, the picture's rounded corners

void MessageCard::select(bool)
{
}

void MessageCard::activate(bool activated)
{
    m_pressed = activated;
    paintBackground();
}

void MessageCard::reset(bool, bool activated)
{
    m_pressed = activated;
    paintBackground();
}

// Everything the text size and the theme's colours decide, set again only when one of them changed
// or a part was made. The gap goes above each card: in the bottom-to-top list that also spaces the
// first card from its day's header (Q10, 2026-10-31). It and the card's padding grow with the text
// size (Q10, 2026-11-01: 140 % looked cramped). 2 px over 1.5 and 1 du: the text sat a little
// tight to the card's edges (Q10). Cards are recycled between ours and theirs, so the side is one of the causes:
// the quiet text is Telegram's blue-grey on our bubble.
void MessageCard::restyle(bool out)
{
    const float scale = Card::textScale(m_list);
    const int body = Card::bodySize(m_list);
    const int small = Card::smallSize(m_list);
    const Color plain = m_ui->palette()->plain();
    const bool round = Card::isRound(m_list);
    if (m_styled && scale == m_scale && body == m_body && small == m_small && plain == m_plain && out == m_restyledOut
        && round == m_round)
        return;
    m_styled = true;
    m_restyledOut = out;
    m_round = round;
    m_scale = scale;
    m_body = body;
    m_small = small;
    m_plain = plain;
    // [...] app logic left out: the card image for Round mode, the bars' tints
    const float side = Card::side(m_ui, scale, round);
    // The gap above the card: less in Round mode, its cards sat too far apart (the user, 2026-11-19).
    m_root->setTopPadding(m_ui->du((round ? 1 : 1.5) * scale));
    m_card->setLeftPadding(side);
    m_card->setRightPadding(side);
    m_card->setTopPadding(m_ui->du(1 * scale)); // 2 px less: the name and time a little higher (Q10, 0.0.1.212)
    m_card->setBottomPadding(m_ui->du(1 * scale) + m_ui->px(2));
    // [...] app logic left out: the head's paddings, the text styles of every part
}

// [...] app logic left out: makeSender, makeTile, tileSpace

// on: the message has a tile at all (a group's incoming one). Then with the run's newest message it shows the picture
// or, with none, the initials on the sender's colour, as the chat list's tile does; every other message of the run keeps
// the column's width and nothing else, an empty spacer. Everything is set on each call: cards are recycled, and a
// picture left from another sender would show.
void MessageCard::fillTile(bool on)
{
    if (!m_avatar)
        return;
    m_avatar->setVisible(on);
    const bool tile = on && flag("showTile");
    const QString photo = tile ? str("senderPhoto") : QString();
    // Round mode: a disc, and the picture's round copy (roundPhotoSoon: made on a worker, "" until it is there). Until then
    // the tile shows as a sender with no picture does, the coloured disc and the initials, not the square picture: the
    // decode, mask and PNG save were 606 of 828 ms of one open's fills on the UI thread (the Classic, 1.0.0.99), and
    // MessageModel::onPreviews draws the card again when the copy is made. Square mode shows the picture as it is.
    const bool round = Card::isRound(m_list);
    const QString disc = round && !photo.isEmpty() ? roundPhotoSoon(photo) : QString();
    const QString shownPhoto = round ? disc : photo;
    if (shownPhoto != m_photoPath) {
        m_photoPath = shownPhoto;
        m_photo->setImageSource(shownPhoto.isEmpty() ? QUrl() : QUrl(QString::fromLatin1("file://") + shownPhoto));
    }
    m_photo->setVisible(!shownPhoto.isEmpty());
    // [...] app logic left out: the tile's disc, initials and colours
}

// [...] app logic left out: makeForwarded, makeQuote

// After the voice, media and album bubbles, before the text.
void MessageCard::makeLabel()
{
    m_label = new Label();
    m_label->setTopMargin(0);
    m_label->setBottomMargin(0);
    m_label->textStyle()->setFontStyle(FontStyle::Italic);
    m_col->insert(m_col->indexOf(m_words), m_label);
    m_styled = false;
}

// [...] app logic left out: makeJoin, makeBang, the reaction chips, fillColumn, timeLead, makeBars, makeTimer

// The time (the star and "edited" before it) and our message's state, as in ChatRows.cpp: sending, sent, delivered,
// read, or a red "!" when it failed; then the foot and the reactions' chips.
void MessageCard::fillFoot()
{
    // [...] app logic left out: the time's text, the self-destruct clock, and which of picture, sticker, album or voice decides the place
    const int place = onPicture ? 2 : onSticker ? 4 : m_words->isVisible() ? 1 : onAlbum ? 5 : m_voice && flag("voice") ? 3 : 0;
    placeTime(place);
    // [...] app logic left out: the time box's top padding, the ticks' size, the "!" of a failed message

    // The column's width at most: the card's less its paddings, and less the time when that is
    // beside it. Without it a long text asks for the whole row and the stack takes the excess from
    // both, so the time is cut.
    if (!m_head) {
        const float most = Card::width(m_ui) - 2 * Card::side(m_ui, m_scale, m_round)
                           - (place == 0 ? m_ui->du(1) + timeWidth() : 0);
        if (most != m_colMax) {
            m_colMax = most;
            m_col->setMaxWidth(most);
        }
    }

    // [...] app logic left out: the accent bars, the tails, the reaction chips
}

// [...] app logic left out: timeWidth, a reckoning of the time and ticks' width

// The time and ticks: at the bottom right of the text, over the blanks that end it; under a voice
// note's waveform, at the right of its length; else beside the column's first line; on a photo with
// no caption at its bottom right, on a dark chip so they read on any picture, as Android shows them. Moved only when the place changes, since
// rows are recycled.
void MessageCard::placeTime(int place)
{
    if (place == m_timePlace)
        return;
    Container *const homes[] = { m_card, m_words, m_media ? m_media->stamp() : 0, m_voice ? m_voice->stamp() : 0,
                                 m_media ? m_media->stickerStamp() : 0, m_album ? m_album->stamp() : 0 };
    if (m_timePlace >= 0)
        homes[m_timePlace]->remove(m_timeBox);
    homes[place]->add(m_timeBox);
    m_timePlace = place;

    // [...] app logic left out: the time box's margins, paddings and chip background for its place
}

// [...] app logic left out: onLaidOut, which reports the card's height as it is laid out

// Also from show() and MessageCards::slideRow: a card reused at the same size and place isn't laid out again, so it says its
// last height itself; a layout that follows with another height reports again, and the page slides by the latest.
// A text of ours slides in as a local row (MessageModel::addLocal, id "local-n") and the engine's row takes its place under its
// own id, possibly before this card has been measured: the row carries the local id as `local`, so the card is still the one.
void MessageCard::reportSlide(const QString &id)
{
    if (id.isEmpty() || (id != str("id") && id != str("local")) || m_laidHeight <= 0)
        return;
    Card::call(m_list, "slideMeasured", m_laidHeight);
}
