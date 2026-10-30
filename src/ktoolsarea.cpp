/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ktoolsarea.h"

#include "kcolorscheme.h"

#include <KColorUtils>
#include <KConfigGroup>

#include <QMap>
#include <QRegularExpression>

#include <algorithm>

namespace {
const QStringList hideTitleBarChoices = {QStringLiteral("Never"),
    QStringLiteral("Always"),
    QStringLiteral("Maximized"),
    QStringLiteral("AnyMaximization"),
    QStringLiteral("KeptBehind"),
    QStringLiteral("AnyMaximizationOrKeptBehind")};

const QString toolsAreaGroup = QStringLiteral("ToolsArea");

// The built-in default exceptions, before the user's. Default Application 0: VirtualBox
// draws its own header area, so it stays opaque.
QList<KToolsArea::Exception> defaultExceptions()
{
    KToolsArea::Exception virtualBox;
    virtualBox.pattern = QStringLiteral("VirtualBox.*");
    virtualBox.isDefault = true;
    virtualBox.overrides.insert(QStringLiteral("ApplyOpacityToHeader"), false);
    return {virtualBox};
}

QString defaultGroupName(int index)
{
    return QStringLiteral("Default Application %1").arg(index);
}

int hideTitleBarIndex(const QString& value)
{
    for (int i = 0; i < hideTitleBarChoices.size(); ++i) {
        if (value.compare(hideTitleBarChoices.at(i), Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return 0;
}

// The keys present in group onto settings; Enabled only for the [ToolsArea] group itself
// (an exception's Enabled is its own switch).
void readKeys(const KConfigGroup& group, KToolsArea::Settings& settings, bool includeEnabled)
{
    if (includeEnabled) {
        settings.enabled = group.readEntry("Enabled", settings.enabled);
    }
    settings.activeOpacity = std::clamp(group.readEntry("ActiveOpacity", settings.activeOpacity), 0, 100);
    settings.inactiveOpacity = std::clamp(group.readEntry("InactiveOpacity", settings.inactiveOpacity), 0, 100);
    settings.applyOpacityToHeader = group.readEntry("ApplyOpacityToHeader", settings.applyOpacityToHeader);
    settings.matchApplicationColor = group.readEntry("MatchApplicationColor", settings.matchApplicationColor);
    settings.blurBehind = group.readEntry("BlurBehind", settings.blurBehind);
    settings.opaqueWhenMaximized = group.readEntry("OpaqueWhenMaximized", settings.opaqueWhenMaximized);
    settings.separatorColor = group.readEntry("SeparatorColor", settings.separatorColor);
    if (group.hasKey("HideTitleBar")) {
        settings.hideTitleBar = hideTitleBarIndex(group.readEntry("HideTitleBar", QString()));
    }
}

// An exception's overrides onto settings.
void applyOverrides(const QVariantMap& overrides, KToolsArea::Settings& settings)
{
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
        const QString& key = it.key();
        if (key == QLatin1String("ActiveOpacity")) {
            settings.activeOpacity = std::clamp(it.value().toInt(), 0, 100);
        } else if (key == QLatin1String("InactiveOpacity")) {
            settings.inactiveOpacity = std::clamp(it.value().toInt(), 0, 100);
        } else if (key == QLatin1String("ApplyOpacityToHeader")) {
            settings.applyOpacityToHeader = it.value().toBool();
        } else if (key == QLatin1String("MatchApplicationColor")) {
            settings.matchApplicationColor = it.value().toBool();
        } else if (key == QLatin1String("BlurBehind")) {
            settings.blurBehind = it.value().toBool();
        } else if (key == QLatin1String("OpaqueWhenMaximized")) {
            settings.opaqueWhenMaximized = it.value().toBool();
        } else if (key == QLatin1String("SeparatorColor")) {
            settings.separatorColor = it.value().value<QColor>();
        } else if (key == QLatin1String("HideTitleBar")) {
            settings.hideTitleBar = hideTitleBarIndex(it.value().toString());
        }
    }
}

// The exception keys present in group, typed.
QVariantMap readOverrides(const KConfigGroup& group)
{
    QVariantMap overrides;
    for (const char* key : {"ActiveOpacity", "InactiveOpacity"}) {
        if (group.hasKey(key)) {
            overrides.insert(QString::fromLatin1(key), std::clamp(group.readEntry(key, 100), 0, 100));
        }
    }
    for (const char* key : {"ApplyOpacityToHeader", "MatchApplicationColor", "BlurBehind", "OpaqueWhenMaximized"}) {
        if (group.hasKey(key)) {
            overrides.insert(QString::fromLatin1(key), group.readEntry(key, false));
        }
    }
    if (group.hasKey("SeparatorColor")) {
        overrides.insert(QStringLiteral("SeparatorColor"), group.readEntry("SeparatorColor", QColor()));
    }
    if (group.hasKey("HideTitleBar")) {
        overrides.insert(QStringLiteral("HideTitleBar"), group.readEntry("HideTitleBar", QString()));
    }
    return overrides;
}

// The [ToolsArea][Application <n>] group names in numeric order.
QStringList numberedGroups(const KConfigGroup& toolsArea)
{
    QMap<int, QString> names;
    for (const QString& name : toolsArea.groupList()) {
        bool ok = false;
        const int index = name.startsWith(QLatin1String("Application ")) ? name.mid(12).toInt(&ok) : 0;
        if (ok) {
            names.insert(index, name);
        }
    }
    return names.values();
}

bool matches(const QString& pattern, const QString& name)
{
    return !pattern.isEmpty() && QRegularExpression(pattern, QRegularExpression::MultilineOption).match(name).hasMatch();
}
}

KToolsArea::Settings KToolsArea::settings(const KSharedConfigPtr& scheme, const QString& name)
{
    Settings settings;
    readKeys(scheme->group(toolsAreaGroup), settings, true);
    if (name.isEmpty()) {
        return settings;
    }
    for (const Exception& exception : exceptions(scheme)) {
        if (exception.enabled && matches(exception.pattern, name)) {
            applyOverrides(exception.overrides, settings);
            break;
        }
    }
    return settings;
}

KToolsArea::Colors KToolsArea::colors(const KSharedConfigPtr& scheme, const Settings& settings)
{
    Colors colors;
    QPalette palette = KColorScheme::createApplicationPalette(scheme);
    const bool schemeHasHeader = KColorScheme::isColorSetSupported(scheme, KColorScheme::Header);
    if (schemeHasHeader) {
        for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            const KColorScheme header(group, KColorScheme::Header, scheme);
            palette.setBrush(group, QPalette::Window, header.background());
            palette.setBrush(group, QPalette::WindowText, header.foreground());
        }
    }
    colors.headerColors = schemeHasHeader || settings.matchApplicationColor;
    if (colors.headerColors && settings.applyOpacityToHeader) {
        for (const auto group : {QPalette::Active, QPalette::Inactive}) {
            QColor color = palette.color(group, QPalette::Window);
            color.setAlphaF(qreal(group == QPalette::Active ? settings.activeOpacity : settings.inactiveOpacity) / 100);
            palette.setColor(group, QPalette::Window, color);
        }
    }
    colors.palette = palette;
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        colors.translucent = colors.translucent || palette.color(group, QPalette::Window).alpha() < 255;
    }
    colors.separatorColor = settings.separatorColor;
    if (!colors.separatorColor.isValid()) {
        // Window and WindowText mixed 20 % (the Active group), faint without header colours.
        colors.separatorColor = KColorUtils::mix(palette.color(QPalette::Active, QPalette::Window), palette.color(QPalette::Active, QPalette::WindowText), 0.20);
        if (!colors.headerColors) {
            colors.separatorColor.setAlphaF(colors.separatorColor.alphaF() * 0.3);
        }
    }
    return colors;
}

void KToolsArea::writeSettings(const KSharedConfigPtr& scheme, const Settings& settings)
{
    KConfigGroup group = scheme->group(toolsAreaGroup);
    const auto flags = KConfig::Normal | KConfig::Notify;
    group.writeEntry("Enabled", true, flags);
    group.writeEntry("ActiveOpacity", settings.activeOpacity, flags);
    group.writeEntry("InactiveOpacity", settings.inactiveOpacity, flags);
    group.writeEntry("ApplyOpacityToHeader", settings.applyOpacityToHeader, flags);
    group.writeEntry("MatchApplicationColor", settings.matchApplicationColor, flags);
    group.writeEntry("BlurBehind", settings.blurBehind, flags);
    group.writeEntry("OpaqueWhenMaximized", settings.opaqueWhenMaximized, flags);
    if (settings.separatorColor.isValid()) {
        group.writeEntry("SeparatorColor", settings.separatorColor, flags);
    } else {
        group.deleteEntry("SeparatorColor", flags);
    }
    if (settings.hideTitleBar > 0 && settings.hideTitleBar < hideTitleBarChoices.size()) {
        group.writeEntry("HideTitleBar", hideTitleBarChoices.at(settings.hideTitleBar), flags);
    } else {
        group.deleteEntry("HideTitleBar", flags);
    }
}

QList<KToolsArea::Exception> KToolsArea::exceptions(const KSharedConfigPtr& scheme)
{
    const KConfigGroup toolsArea = scheme->group(toolsAreaGroup);
    QList<Exception> list = defaultExceptions();
    for (int i = 0; i < list.size(); ++i) {
        list[i].enabled = toolsArea.group(defaultGroupName(i)).readEntry("Enabled", true);
    }
    for (const QString& name : numberedGroups(toolsArea)) {
        const KConfigGroup group = toolsArea.group(name);
        Exception exception;
        exception.pattern = group.readEntry("Pattern", QString());
        exception.enabled = group.readEntry("Enabled", true);
        exception.overrides = readOverrides(group);
        list.append(exception);
    }
    return list;
}

void KToolsArea::writeExceptions(const KSharedConfigPtr& scheme, const QList<Exception>& exceptions)
{
    KConfigGroup toolsArea = scheme->group(toolsAreaGroup);
    const auto flags = KConfig::Normal | KConfig::Notify;
    for (const QString& name : toolsArea.groupList()) {
        if (name.startsWith(QLatin1String("Application ")) || name.startsWith(QLatin1String("Default Application "))) {
            toolsArea.deleteGroup(name, flags);
        }
    }
    int defaultIndex = 0;
    int index = 0;
    for (const Exception& exception : exceptions) {
        if (exception.isDefault) {
            // Only whether it is switched off is stored.
            if (!exception.enabled) {
                toolsArea.group(defaultGroupName(defaultIndex)).writeEntry("Enabled", false, flags);
            }
            ++defaultIndex;
            continue;
        }
        if (exception.pattern.isEmpty()) {
            continue;
        }
        KConfigGroup group = toolsArea.group(QStringLiteral("Application %1").arg(index++));
        group.writeEntry("Pattern", exception.pattern, flags);
        group.writeEntry("Enabled", exception.enabled, flags);
        for (auto it = exception.overrides.cbegin(); it != exception.overrides.cend(); ++it) {
            if (it.value().isValid()) {
                group.writeEntry(it.key().toLatin1().constData(), it.value(), flags);
            }
        }
    }
}

QString KToolsArea::snapshot(const KSharedConfigPtr& scheme)
{
    const KConfigGroup toolsArea = scheme->group(toolsAreaGroup);
    QString text;
    const auto add = [&text](const KConfigGroup& group) {
        const QMap<QString, QString> entries = group.entryMap();
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            text += group.name() + QLatin1Char('/') + it.key() + QLatin1Char('=') + it.value() + QLatin1Char('\n');
        }
    };
    add(toolsArea);
    for (const QString& name : toolsArea.groupList()) {
        add(toolsArea.group(name));
    }
    return text;
}

bool KToolsArea::migrateFromSilver(const KSharedConfigPtr& scheme)
{
    if (scheme->group(toolsAreaGroup).hasKey("Enabled")) {
        return false;
    }
    const KSharedConfigPtr silverrc = KSharedConfig::openConfig(QStringLiteral("silver/silverrc"));

    // Silver's earlier [TitleBarOpacity] and [Windeco] keys, with Silver's defaults.
    KConfigGroup opacity = silverrc->group(QStringLiteral("TitleBarOpacity"));
    KConfigGroup windeco = silverrc->group(QStringLiteral("Windeco"));
    Settings settings;
    settings.activeOpacity = std::clamp(opacity.readEntry("ActiveTitleBarOpacity", 100), 0, 100);
    settings.inactiveOpacity = std::clamp(opacity.readEntry("InactiveTitleBarOpacity", 100), 0, 100);
    settings.opaqueWhenMaximized = opacity.readEntry("OpaqueMaximizedTitleBars", true);
    settings.blurBehind = opacity.readEntry("BlurTransparentTitleBars", true);
    settings.applyOpacityToHeader = opacity.readEntry("ApplyOpacityToHeader", true);
    settings.matchApplicationColor = windeco.readEntry("MatchTitleBarToApplicationColor", false);
    writeSettings(scheme, settings);

    // Silver's exceptions that changed the transparency become exceptions here; the
    // rest of each stays Silver's.
    QList<Exception> list = defaultExceptions();
    list[0].enabled = silverrc->group(QStringLiteral("Default Windeco Exception 0")).readEntry("Enabled", true);
    for (int index = 0;; ++index) {
        const QString name = QStringLiteral("Windeco Exception %1").arg(index);
        if (!silverrc->hasGroup(name)) {
            break;
        }
        KConfigGroup group = silverrc->group(name);
        const QString type = group.readEntry("ExceptionWindowPropertyType", QString());
        const bool byClass = type.isEmpty() || type == QLatin1String("0") || type == QLatin1String("ExceptionWindowClassName");
        const QString propertyPattern = group.readEntry("ExceptionWindowPropertyPattern", QString());
        // Exceptions match the application name or window class; title rules can't move.
        const QString pattern = byClass && !propertyPattern.isEmpty() ? propertyPattern : group.readEntry("ExceptionProgramNamePattern", QString());
        Exception exception;
        exception.pattern = pattern;
        exception.enabled = group.readEntry("Enabled", true);
        if (group.readEntry("OpaqueTitleBar", false)) {
            exception.overrides.insert(QStringLiteral("ActiveOpacity"), 100);
            exception.overrides.insert(QStringLiteral("InactiveOpacity"), 100);
        }
        if (group.readEntry("PreventApplyOpacityToHeader", false)) {
            exception.overrides.insert(QStringLiteral("ApplyOpacityToHeader"), false);
        }
        if (group.readEntry("ExceptionMatchTitleBarToApplicationColor", false)) {
            exception.overrides.insert(QStringLiteral("MatchApplicationColor"), true);
        }
        if (!pattern.isEmpty() && !exception.overrides.isEmpty()) {
            list.append(exception);
        }
        for (const char* key : {"OpaqueTitleBar", "PreventApplyOpacityToHeader", "ExceptionMatchTitleBarToApplicationColor"}) {
            group.deleteEntry(key);
        }
    }
    writeExceptions(scheme, list);
    scheme->sync();

    // Moved, not copied.
    for (const char* key : {"ActiveTitleBarOpacity",
             "InactiveTitleBarOpacity",
             "OverrideActiveTitleBarOpacity",
             "OverrideInactiveTitleBarOpacity",
             "OpaqueMaximizedTitleBars",
             "BlurTransparentTitleBars",
             "ApplyOpacityToHeader"}) {
        opacity.deleteEntry(key);
    }
    windeco.deleteEntry("MatchTitleBarToApplicationColor");
    silverrc->sync();
    return true;
}
