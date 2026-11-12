/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The [ToolsArea] rules every reader shares: keys and defaults, colours, exceptions and
// the built-in default ones, writing, and the one-time move from Silver's settings.

#include <KToolsArea>

#include <KColorUtils>
#include <KConfigGroup>

#include <QDir>
#include <QStandardPaths>
#include <QTest>

namespace {
const QColor headerBackground(200, 210, 220);
const QColor headerForeground(10, 20, 30);
const QColor inactiveHeaderBackground(180, 190, 200);
const QColor windowBackground(239, 240, 241);
const QColor windowForeground(35, 38, 41);

QString rgb(const QColor& color)
{
    return QStringLiteral("%1,%2,%3").arg(color.red()).arg(color.green()).arg(color.blue());
}

QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}
}

class KToolsAreaTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void testDefaults();
    void testKeys();
    void testOpacity();
    void testApplyOpacityToHeaderOff();
    void testNoHeaderColorsIsFaintAndOpaque();
    void testMatchApplicationColor();
    void testSeparatorColorKey();
    void testExceptions();
    void testDefaultVirtualBoxException();
    void testWriteExceptionsRoundTrip();
    void testWriteSettings();
    void testSnapshot();
    void testMigrateFromSilver();
    void testFollowDecoration_data();
    void testFollowDecoration();
    void testMenuOpacity();
    void testMigrateMenuOpacity_data();
    void testMigrateMenuOpacity();

private:
    void writeScheme(bool header);
    KConfigGroup toolsArea();

    KSharedConfigPtr m_scheme;
};

void KToolsAreaTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void KToolsAreaTest::init()
{
    writeScheme(true);
}

void KToolsAreaTest::writeScheme(bool header)
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QDir(config).mkpath(QStringLiteral("silver"));
    const QString path = config + QStringLiteral("/kdeglobals");
    m_scheme = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
    m_scheme->markAsClean();
    QFile::remove(path);
    KConfig file(path, KConfig::SimpleConfig);
    file.group(QStringLiteral("ColorEffects:Inactive")).writeEntry("Enable", false);
    file.group(QStringLiteral("ColorEffects:Inactive")).writeEntry("ChangeSelectionColor", false);
    for (const QString& set : {QStringLiteral("Colors:Window"), QStringLiteral("Colors:View"), QStringLiteral("Colors:Button")}) {
        file.group(set).writeEntry("BackgroundNormal", rgb(windowBackground));
        file.group(set).writeEntry("ForegroundNormal", rgb(windowForeground));
    }
    if (header) {
        KConfigGroup group = file.group(QStringLiteral("Colors:Header"));
        group.writeEntry("BackgroundNormal", rgb(headerBackground));
        group.writeEntry("ForegroundNormal", rgb(headerForeground));
        KConfigGroup inactive = group.group(QStringLiteral("Inactive"));
        inactive.writeEntry("BackgroundNormal", rgb(inactiveHeaderBackground));
        inactive.writeEntry("ForegroundNormal", rgb(headerForeground));
    }
    file.sync();
    m_scheme->reparseConfiguration();
}

KConfigGroup KToolsAreaTest::toolsArea()
{
    return m_scheme->group(QStringLiteral("ToolsArea"));
}

void KToolsAreaTest::testDefaults()
{
    const KToolsArea::Settings settings = KToolsArea::settings(m_scheme, QStringLiteral("app"));
    QVERIFY(!settings.enabled);
    QCOMPARE(settings.activeOpacity, 100);
    QCOMPARE(settings.inactiveOpacity, 100);
    QVERIFY(settings.applyOpacityToHeader);
    QVERIFY(!settings.matchApplicationColor);
    QVERIFY(settings.blurBehind);
    QVERIFY(settings.opaqueWhenMaximized);
    QVERIFY(!settings.separatorColor.isValid());
    QCOMPARE(settings.hideTitleBar, 0);

    const KToolsArea::Colors colors = KToolsArea::colors(m_scheme, settings);
    QCOMPARE(colors.palette.color(QPalette::Active, QPalette::Window), headerBackground);
    QCOMPARE(colors.palette.color(QPalette::Inactive, QPalette::Window), inactiveHeaderBackground);
    QCOMPARE(colors.separatorColor, KColorUtils::mix(headerBackground, headerForeground, 0.20));
    QVERIFY(colors.headerColors);
    QVERIFY(!colors.translucent);
}

void KToolsAreaTest::testKeys()
{
    toolsArea().writeEntry("Enabled", true);
    toolsArea().writeEntry("BlurBehind", false);
    toolsArea().writeEntry("OpaqueWhenMaximized", false);
    toolsArea().writeEntry("HideTitleBar", QStringLiteral("Maximized"));
    const KToolsArea::Settings settings = KToolsArea::settings(m_scheme);
    QVERIFY(settings.enabled);
    QVERIFY(!settings.blurBehind);
    QVERIFY(!settings.opaqueWhenMaximized);
    QCOMPARE(settings.hideTitleBar, 2);
}

void KToolsAreaTest::testOpacity()
{
    toolsArea().writeEntry("ActiveOpacity", 60);
    toolsArea().writeEntry("InactiveOpacity", 40);
    const KToolsArea::Colors colors = KToolsArea::colors(m_scheme, KToolsArea::settings(m_scheme));
    QCOMPARE(colors.palette.color(QPalette::Active, QPalette::Window), withAlpha(headerBackground, 0.6));
    QCOMPARE(colors.palette.color(QPalette::Inactive, QPalette::Window), withAlpha(inactiveHeaderBackground, 0.4));
    QVERIFY(colors.translucent);
}

void KToolsAreaTest::testApplyOpacityToHeaderOff()
{
    toolsArea().writeEntry("ActiveOpacity", 60);
    toolsArea().writeEntry("ApplyOpacityToHeader", false);
    const KToolsArea::Colors colors = KToolsArea::colors(m_scheme, KToolsArea::settings(m_scheme));
    QCOMPARE(colors.palette.color(QPalette::Active, QPalette::Window), headerBackground);
    QVERIFY(!colors.translucent);
}

void KToolsAreaTest::testNoHeaderColorsIsFaintAndOpaque()
{
    writeScheme(false);
    toolsArea().writeEntry("ActiveOpacity", 60);
    const KToolsArea::Colors colors = KToolsArea::colors(m_scheme, KToolsArea::settings(m_scheme));
    QCOMPARE(colors.palette.color(QPalette::Active, QPalette::Window), windowBackground);
    QVERIFY(!colors.headerColors);
    QVERIFY(!colors.translucent);
    QColor faint = KColorUtils::mix(windowBackground, windowForeground, 0.20);
    faint.setAlphaF(faint.alphaF() * 0.3);
    QCOMPARE(colors.separatorColor, faint);
}

void KToolsAreaTest::testMatchApplicationColor()
{
    writeScheme(false);
    toolsArea().writeEntry("MatchApplicationColor", true);
    toolsArea().writeEntry("ActiveOpacity", 50);
    const KToolsArea::Colors colors = KToolsArea::colors(m_scheme, KToolsArea::settings(m_scheme));
    QCOMPARE(colors.palette.color(QPalette::Active, QPalette::Window), withAlpha(windowBackground, 0.5));
    QCOMPARE(colors.separatorColor, KColorUtils::mix(withAlpha(windowBackground, 0.5), windowForeground, 0.20));
}

void KToolsAreaTest::testSeparatorColorKey()
{
    toolsArea().writeEntry("SeparatorColor", QColor(1, 2, 3));
    QCOMPARE(KToolsArea::colors(m_scheme, KToolsArea::settings(m_scheme)).separatorColor, QColor(1, 2, 3));
}

void KToolsAreaTest::testExceptions()
{
    toolsArea().writeEntry("ActiveOpacity", 60);
    KConfigGroup disabled = toolsArea().group(QStringLiteral("Application 0"));
    disabled.writeEntry("Pattern", QStringLiteral("konsole"));
    disabled.writeEntry("Enabled", false);
    disabled.writeEntry("ActiveOpacity", 10);
    // Numeric order: 2 before 10.
    KConfigGroup tenth = toolsArea().group(QStringLiteral("Application 10"));
    tenth.writeEntry("Pattern", QStringLiteral("konsole"));
    tenth.writeEntry("ActiveOpacity", 30);
    KConfigGroup second = toolsArea().group(QStringLiteral("Application 2"));
    second.writeEntry("Pattern", QStringLiteral("konsole"));
    second.writeEntry("ActiveOpacity", 70);
    second.writeEntry("HideTitleBar", QStringLiteral("Always"));

    const KToolsArea::Settings konsole = KToolsArea::settings(m_scheme, QStringLiteral("konsole"));
    QCOMPARE(konsole.activeOpacity, 70);
    QCOMPARE(konsole.hideTitleBar, 1);
    // An exception's Enabled doesn't switch the tools area itself.
    QVERIFY(!konsole.enabled);
    QCOMPARE(KToolsArea::settings(m_scheme, QStringLiteral("dolphin")).activeOpacity, 60);
    // No name, no exceptions.
    QCOMPARE(KToolsArea::settings(m_scheme).activeOpacity, 60);
}

void KToolsAreaTest::testDefaultVirtualBoxException()
{
    const QList<KToolsArea::Exception> list = KToolsArea::exceptions(m_scheme);
    QVERIFY(!list.isEmpty());
    QVERIFY(list.first().isDefault);
    QCOMPARE(list.first().pattern, QStringLiteral("VirtualBox.*"));
    QVERIFY(list.first().enabled);

    QVERIFY(!KToolsArea::settings(m_scheme, QStringLiteral("VirtualBoxVM")).applyOpacityToHeader);
    toolsArea().group(QStringLiteral("Default Application 0")).writeEntry("Enabled", false);
    QVERIFY(KToolsArea::settings(m_scheme, QStringLiteral("VirtualBoxVM")).applyOpacityToHeader);
}

void KToolsAreaTest::testWriteExceptionsRoundTrip()
{
    QList<KToolsArea::Exception> list = KToolsArea::exceptions(m_scheme);
    list[0].enabled = false;
    KToolsArea::Exception kate;
    kate.pattern = QStringLiteral("kate");
    kate.overrides.insert(QStringLiteral("ActiveOpacity"), 80);
    kate.overrides.insert(QStringLiteral("BlurBehind"), false);
    list.append(kate);
    KToolsArea::Exception empty; // no pattern: dropped
    list.append(empty);
    KToolsArea::writeExceptions(m_scheme, list);

    const QList<KToolsArea::Exception> back = KToolsArea::exceptions(m_scheme);
    QCOMPARE(back.size(), 2);
    QVERIFY(!back[0].enabled);
    QCOMPARE(back[1].pattern, QStringLiteral("kate"));
    QCOMPARE(back[1].overrides.value(QStringLiteral("ActiveOpacity")).toInt(), 80);
    QCOMPARE(back[1].overrides.value(QStringLiteral("BlurBehind")).toBool(), false);
    QVERIFY(!back[1].overrides.contains(QStringLiteral("InactiveOpacity")));
}

void KToolsAreaTest::testWriteSettings()
{
    KToolsArea::Settings settings;
    settings.activeOpacity = 60;
    settings.blurBehind = false;
    KToolsArea::writeSettings(m_scheme, settings);
    const KToolsArea::Settings back = KToolsArea::settings(m_scheme);
    QVERIFY(back.enabled);
    QCOMPARE(back.activeOpacity, 60);
    QVERIFY(!back.blurBehind);
}

void KToolsAreaTest::testSnapshot()
{
    const QString before = KToolsArea::snapshot(m_scheme);
    toolsArea().group(QStringLiteral("Application 0")).writeEntry("Pattern", QStringLiteral("x"));
    QVERIFY(KToolsArea::snapshot(m_scheme) != before);
}

void KToolsAreaTest::testMigrateFromSilver()
{
    const KSharedConfigPtr silverrc = KSharedConfig::openConfig(QStringLiteral("silver/silverrc"));
    silverrc->group(QStringLiteral("TitleBarOpacity")).writeEntry("ActiveTitleBarOpacity", 60);
    silverrc->group(QStringLiteral("TitleBarOpacity")).writeEntry("BlurTransparentTitleBars", false);
    silverrc->group(QStringLiteral("Windeco")).writeEntry("MatchTitleBarToApplicationColor", true);
    silverrc->group(QStringLiteral("Windeco")).writeEntry("AnimationsEnabled", false);
    KConfigGroup byClass = silverrc->group(QStringLiteral("Windeco Exception 0"));
    byClass.writeEntry("ExceptionWindowPropertyPattern", QStringLiteral("konsole"));
    byClass.writeEntry("OpaqueTitleBar", true);
    byClass.writeEntry("HideTitleBar", QStringLiteral("Always"));
    KConfigGroup byTitle = silverrc->group(QStringLiteral("Windeco Exception 1"));
    byTitle.writeEntry("ExceptionWindowPropertyType", QStringLiteral("ExceptionWindowTitle"));
    byTitle.writeEntry("ExceptionWindowPropertyPattern", QStringLiteral("Some title"));
    byTitle.writeEntry("ExceptionProgramNamePattern", QStringLiteral("dolphin"));
    byTitle.writeEntry("PreventApplyOpacityToHeader", true);
    silverrc->group(QStringLiteral("Default Windeco Exception 0")).writeEntry("Enabled", false);
    silverrc->sync();

    QVERIFY(KToolsArea::migrateFromSilver(m_scheme));
    const KToolsArea::Settings settings = KToolsArea::settings(m_scheme);
    QVERIFY(settings.enabled);
    QCOMPARE(settings.activeOpacity, 60);
    QVERIFY(!settings.blurBehind);
    QVERIFY(settings.matchApplicationColor);

    const QList<KToolsArea::Exception> list = KToolsArea::exceptions(m_scheme);
    QCOMPARE(list.size(), 3);
    QVERIFY(!list[0].enabled); // VirtualBox was switched off in Silver
    QCOMPARE(list[1].pattern, QStringLiteral("konsole"));
    QCOMPARE(list[1].overrides.value(QStringLiteral("ActiveOpacity")).toInt(), 100);
    QVERIFY(!list[1].overrides.contains(QStringLiteral("HideTitleBar"))); // stays Silver's
    QCOMPARE(list[2].pattern, QStringLiteral("dolphin")); // a title rule falls back to the program name
    QCOMPARE(list[2].overrides.value(QStringLiteral("ApplyOpacityToHeader")).toBool(), false);

    // Moved, not copied; Silver's own settings stay.
    silverrc->reparseConfiguration();
    QVERIFY(!silverrc->group(QStringLiteral("TitleBarOpacity")).hasKey("ActiveTitleBarOpacity"));
    QVERIFY(!silverrc->group(QStringLiteral("Windeco")).hasKey("MatchTitleBarToApplicationColor"));
    QVERIFY(silverrc->group(QStringLiteral("Windeco")).hasKey("AnimationsEnabled"));
    QVERIFY(!silverrc->group(QStringLiteral("Windeco Exception 0")).hasKey("OpaqueTitleBar"));
    QCOMPARE(silverrc->group(QStringLiteral("Windeco Exception 0")).readEntry("HideTitleBar", QString()), QStringLiteral("Always"));

    // Once only.
    toolsArea().writeEntry("ActiveOpacity", 80);
    QVERIFY(!KToolsArea::migrateFromSilver(m_scheme));
    QCOMPARE(KToolsArea::settings(m_scheme).activeOpacity, 80);
}

void KToolsAreaTest::testFollowDecoration_data()
{
    // Enabled before (invalid: no key), DisabledByDecoration before, decoration supports it;
    // then whether anything changed, Enabled and DisabledByDecoration after.
    QTest::addColumn<QVariant>("enabled");
    QTest::addColumn<bool>("disabledByDecoration");
    QTest::addColumn<bool>("supported");
    QTest::addColumn<bool>("changed");
    QTest::addColumn<bool>("enabledAfter");
    QTest::addColumn<bool>("disabledByDecorationAfter");

    QTest::newRow("unsupported turns an enabled area off") << QVariant(true) << false << false << true << false << true;
    QTest::newRow("unsupported leaves a disabled area") << QVariant(false) << false << false << false << false << false;
    QTest::newRow("unsupported leaves an unset area") << QVariant() << false << false << false << false << false;
    QTest::newRow("unsupported keeps its earlier record") << QVariant(false) << true << false << false << false << true;
    QTest::newRow("supported restores what a decoration turned off") << QVariant(false) << true << true << true << true << false;
    QTest::newRow("supported keeps an area the user turned off") << QVariant(false) << false << true << false << false << false;
    QTest::newRow("supported leaves an enabled area") << QVariant(true) << false << true << false << true << false;
}

void KToolsAreaTest::testFollowDecoration()
{
    QFETCH(QVariant, enabled);
    QFETCH(bool, disabledByDecoration);
    QFETCH(bool, supported);
    QFETCH(bool, changed);
    QFETCH(bool, enabledAfter);
    QFETCH(bool, disabledByDecorationAfter);

    if (enabled.isValid()) {
        toolsArea().writeEntry("Enabled", enabled.toBool());
    }
    if (disabledByDecoration) {
        toolsArea().writeEntry("DisabledByDecoration", true);
    }
    toolsArea().writeEntry("ActiveOpacity", 70);

    QCOMPARE(KToolsArea::followDecoration(m_scheme, supported), changed);
    QCOMPARE(KToolsArea::settings(m_scheme).enabled, enabledAfter);
    QCOMPARE(toolsArea().readEntry("DisabledByDecoration", false), disabledByDecorationAfter);
    // Only switched: the user's settings stay for when it is turned back on.
    QCOMPARE(KToolsArea::settings(m_scheme).activeOpacity, 70);
    if (!enabled.isValid() && !changed) {
        QVERIFY(!toolsArea().hasKey("Enabled"));
    }
}

void KToolsAreaTest::testMenuOpacity()
{
    QCOMPARE(KToolsArea::menuOpacity(m_scheme), 100);
    KToolsArea::writeMenuOpacity(m_scheme, 70);
    QCOMPARE(KToolsArea::menuOpacity(m_scheme), 70);
    KToolsArea::writeMenuOpacity(m_scheme, 140);
    QCOMPARE(KToolsArea::menuOpacity(m_scheme), 100);
    m_scheme->group(QStringLiteral("Menus")).writeEntry("Opacity", -5);
    QCOMPARE(KToolsArea::menuOpacity(m_scheme), 0);
}

void KToolsAreaTest::testMigrateMenuOpacity_data()
{
    // Silver's and Breeze's MenuOpacity (-1: no key); then the shared value after.
    QTest::addColumn<int>("silver");
    QTest::addColumn<int>("breeze");
    QTest::addColumn<bool>("migrated");
    QTest::addColumn<int>("expected");

    QTest::newRow("neither") << -1 << -1 << false << 100;
    QTest::newRow("Silver") << 60 << -1 << true << 60;
    QTest::newRow("Breeze") << -1 << 80 << true << 80;
    QTest::newRow("Silver wins over Breeze") << 60 << 80 << true << 60;
}

void KToolsAreaTest::testMigrateMenuOpacity()
{
    QFETCH(int, silver);
    QFETCH(int, breeze);
    QFETCH(bool, migrated);
    QFETCH(int, expected);

    const KSharedConfigPtr silverrc = KSharedConfig::openConfig(QStringLiteral("silver/silverrc"));
    const KSharedConfigPtr breezerc = KSharedConfig::openConfig(QStringLiteral("breezerc"));
    for (const auto& [config, value] : {std::pair(silverrc, silver), std::pair(breezerc, breeze)}) {
        KConfigGroup style = config->group(QStringLiteral("Style"));
        style.deleteEntry("MenuOpacity");
        if (value >= 0) {
            style.writeEntry("MenuOpacity", value);
        }
        style.writeEntry("Other", true);
        config->sync();
    }

    QCOMPARE(KToolsArea::migrateMenuOpacity(m_scheme), migrated);
    QCOMPARE(KToolsArea::menuOpacity(m_scheme), expected);

    // Moved from both styles; their other settings stay.
    for (const KSharedConfigPtr& config : {silverrc, breezerc}) {
        config->reparseConfiguration();
        QVERIFY(!config->group(QStringLiteral("Style")).hasKey("MenuOpacity"));
        QVERIFY(config->group(QStringLiteral("Style")).readEntry("Other", false));
    }

    // Once only.
    if (migrated) {
        silverrc->group(QStringLiteral("Style")).writeEntry("MenuOpacity", 10);
        silverrc->sync();
        QVERIFY(!KToolsArea::migrateMenuOpacity(m_scheme));
        QCOMPARE(KToolsArea::menuOpacity(m_scheme), expected);
    }
}

QTEST_GUILESS_MAIN(KToolsAreaTest)

#include "ktoolsareatest.moc"
