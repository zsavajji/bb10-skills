#include "MediaBubble.h"

#include <bb/cascades/Container>
#include <bb/cascades/DockLayout>
#include <bb/cascades/HorizontalAlignment>
#include <bb/cascades/Image>
#include <bb/cascades/ImageView>
#include <bb/cascades/ImageViewLoadEffect>
#include <bb/cascades/Label>
#include <bb/cascades/ListView>
#include <bb/cascades/ScalingMethod>
#include <bb/cascades/TapHandler>
#include <bb/cascades/TextFormat>
#include <bb/cascades/UIConfig>
#include <bb/cascades/VerticalAlignment>

#include "CardStyle.h"

using namespace bb::cascades;

namespace {

// The QML file's name: the translations of MediaBubble.qml's strings (tengram_it.ts) apply.
const char *const kContext = "MediaBubble";

// A plain label placed in its container's dock.
Label *docked(HorizontalAlignment::Type horizontal, VerticalAlignment::Type vertical)
{
    Label *label = new Label();
    label->setHorizontalAlignment(horizontal);
    label->setVerticalAlignment(vertical);
    label->setTextFormat(TextFormat::Plain);
    return label;
}

// A box over the whole of its container, with its own dock.
Container *cover()
{
    Container *box = new Container();
    box->setHorizontalAlignment(HorizontalAlignment::Fill);
    box->setVerticalAlignment(VerticalAlignment::Fill);
    box->setLayout(new DockLayout());
    return box;
}

// [...] app logic left out: spinner and ring, the parts of a fetch in progress
}

MediaBubble::MediaBubble(ListView *list, bool wide)
    : m_list(list)
    , m_ui(list->ui())
    , m_picture(false)
    , m_sticker(false)
    , m_wide(0)
    , m_styled(false)
    , m_body(0)
    , m_small(0)
{
    // A caption follows a picture, with no margin of its own: the bottom margin is set by show().
    setTopMargin(0);
    m_root = new Container();
    TapHandler *tap = new TapHandler(m_root);
    m_root->addGestureHandler(tap);
    connect(tap, SIGNAL(tapped(bb::cascades::TapEvent *)), this, SLOT(onTap()));

    // A picture: the thumbnail, or the page's colour with its name, and what is over it.
    m_photo = new Container();
    m_photo->setLayout(new DockLayout());
    // Wide: the card's width.
    if (wide) {
        m_wide = Card::width(m_ui);
        m_photo->setHorizontalAlignment(HorizontalAlignment::Fill);
    }
    m_photo->setPreferredWidth(wide ? m_wide : m_ui->du(40));
    m_thumb = new ImageView();
    m_thumb->setHorizontalAlignment(HorizontalAlignment::Fill);
    m_thumb->setVerticalAlignment(VerticalAlignment::Fill);
    m_thumb->setScalingMethod(ScalingMethod::AspectFill);
    m_thumb->setLoadEffect(ImageViewLoadEffect::None);

    // No thumbnail: the card is plain, so the box is the page's colour.
    m_none = cover();
    m_noneText = docked(HorizontalAlignment::Center, VerticalAlignment::Center);
    m_none->add(m_noneText);

    // [...] app logic left out: the play mark, the length chip, the spoiler cover, the dimmer, the corner masks, the time's corner

    // [...] app logic left out: the sticker's box and the file row (icon, name, size, spinner, ring)

    m_root->add(m_photo);
    m_root->add(m_stickerBox);
    m_root->add(m_stickerText);
    m_root->add(m_file);
    setRoot(m_root);

    // The fetch, as the QML bound it.
    if (!connect(list, SIGNAL(mediaLoadingIdChanged()), this, SLOT(refresh())))
        qWarning("cards: the list has no mediaLoadingIdChanged");
    if (!connect(list, SIGNAL(mediaProgressChanged()), this, SLOT(refresh()))
        || !connect(list, SIGNAL(mediaProgressTextChanged()), this, SLOT(refresh())))
        qWarning("cards: the list has no mediaProgressChanged");
    if (!connect(list, SIGNAL(mediaUploadsChanged()), this, SLOT(refresh())))
        qWarning("cards: the list has no mediaUploadsChanged");
}

// [...] app logic left out: setCorner and restyle

void MediaBubble::show(const QVariantMap &msg)
{
    m_msg = msg;
    const QString media = str("media");
    const bool location = media == "location";
    const bool contact = media == "contact";
    const bool video = media == "video";
    const bool spoiler = flag("spoiler");
    const bool hasThumb = flag("hasThumb");
    m_picture = media == "photo" || video || (location && hasThumb);
    m_sticker = media == "sticker";
    restyle();

    // A caption follows a picture, with no margin of its own.
    setBottomMargin(str("text").isEmpty() || m_wide ? 0 : m_ui->du(0.5));
    // Air above and below a picture, so it doesn't touch the card's name line or caption (user);
    // none in the picture card, where it spans the card.
    const float air = (m_picture || m_sticker) && !m_wide ? m_ui->du(1) : 0;
    m_root->setTopPadding(air);
    m_root->setBottomPadding(air);

    m_photo->setVisible(m_picture);
    if (m_picture) {
        // A tall photo is cut at 50 du; AspectFill crops it. In the picture card it is square,
        // cropped the same way, and seen whole when opened (the user, 2026-11-08).
        m_photo->setPreferredHeight(m_wide ? m_wide
                                           : qMin(m_ui->du(50), m_ui->du(40) * float(m_msg.value("ratio").toDouble())));
        m_thumb->setVisible(hasThumb);
        m_thumb->setImage(m_msg.value("thumb").value<Image>());
        m_none->setVisible(!hasThumb);
        m_noneText->setText(video ? Card::tr(kContext, QT_TRANSLATE_NOOP("MediaBubble", "Video")) : Card::tr(kContext, QT_TRANSLATE_NOOP("MediaBubble", "Photo")));
        m_play->setVisible(video && !spoiler);
        m_cover->setVisible(spoiler);
    }

    // [...] app logic left out: the sticker branch (size, the TGS player) and the file branch (icon, name, size text)
    refresh();
}

// [...] app logic left out: refresh, which shows the fetch or upload progress read from the list, and onTap
