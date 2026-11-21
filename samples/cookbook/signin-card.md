# The sign-in card (excerpt from bbmusic)

The invoke target in the bar descriptor (BBMusic's bar descriptor): another app starts the UI as a card.

```xml
    <!-- The sign-in card (docs/signin-card.md): the UI's entry point, started as a card. -->
    <invoke-target id="com.zsavajji.bbmusic.card.signin">
        <invoke-target-type>card.picker</invoke-target-type>
        <entry-point-id>BBMusic</entry-point-id>
        <filter>
            <action>com.zsavajji.bbmusic.SHARE_SIGN_IN</action>
            <mime-type>application/json</mime-type>
            <property var="uris" value="data://local"/>
        </filter>
    </invoke-target>

```

The startupMode check in main() (BBMusic's main.cpp): the scene is the card's page instead of the tabs.

```cpp
    bb::system::InvokeManager invoker;
    const bool asCard = invoker.startupMode() == bb::system::ApplicationStartupMode::InvokeCard;
    // [...] app logic left out
    QmlDocument *document = QmlDocument::create(asCard ? "asset:///SignInCard.qml" : "asset:///main.qml").parent(&app);
```
