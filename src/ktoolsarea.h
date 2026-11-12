/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KTOOLSAREA_H
#define KTOOLSAREA_H

#include "kcolorscheme_export.h"

#include <KSharedConfig>

#include <QColor>
#include <QList>
#include <QPalette>
#include <QString>
#include <QVariantMap>

/*!
 * \class KToolsArea
 * \inmodule KColorScheme
 *
 * \brief The window's tools area: the titlebar and the toolbars and main menu bar under
 * it, drawn as one surface.
 *
 * The settings are theme-independent and live in the colour scheme config (kdeglobals,
 * or an application's own scheme) as the [ToolsArea] group:
 * \c Enabled, \c ActiveOpacity, \c InactiveOpacity, \c ApplyOpacityToHeader,
 * \c MatchApplicationColor, \c BlurBehind, \c OpaqueWhenMaximized, \c SeparatorColor,
 * \c HideTitleBar. Exceptions for chosen applications are the
 * [ToolsArea][Application <n>] groups (\c Pattern, a regular expression on the
 * application name or window class, \c Enabled, and any of the keys above), after the
 * built-in default exceptions, which can only be switched off
 * ([ToolsArea][Default Application <n>] \c Enabled=false).
 *
 * Every reader (window decorations, widget styles, the Qt platform theme, settings
 * pages) uses this class, so they agree. Writers send \c KGlobalSettings.notifyChange
 * themselves afterwards; readers update on it.
 */
class KCOLORSCHEME_EXPORT KToolsArea {
public:
    struct Settings {
        bool enabled = false;
        int activeOpacity = 100;
        int inactiveOpacity = 100;
        bool applyOpacityToHeader = true;
        bool matchApplicationColor = false;
        bool blurBehind = true;
        bool opaqueWhenMaximized = true;
        QColor separatorColor; // invalid: derived from the tools area colours
        int hideTitleBar = 0; // 0 Never, 1 Always, 2 Maximized, 3 AnyMaximization, 4 KeptBehind, 5 AnyMaximizationOrKeptBehind
    };

    struct Colors {
        // Window and WindowText: the Header set (the Window set without one), with the
        // opacity as the Window alpha when header colours apply.
        QPalette palette;
        QColor separatorColor;
        // The scheme has a Header set, or MatchApplicationColor is on.
        bool headerColors = false;
        // Any Window colour is translucent.
        bool translucent = false;
    };

    struct Exception {
        QString pattern;
        bool enabled = true;
        // A built-in default exception: only enabled can change.
        bool isDefault = false;
        // Overridden keys (the [ToolsArea] key names) and their values.
        QVariantMap overrides;
    };

    /*!
     * The settings for the application or window class \a name (an empty name: no
     * exceptions).
     */
    static Settings settings(const KSharedConfigPtr& scheme, const QString& name = QString());

    /*!
     * The tools area colours for \a settings with the colour scheme \a scheme.
     */
    static Colors colors(const KSharedConfigPtr& scheme, const Settings& settings);

    /*!
     * Writes \a settings as the [ToolsArea] keys (with Enabled=true).
     */
    static void writeSettings(const KSharedConfigPtr& scheme, const Settings& settings);

    /*!
     * The exceptions, the built-in default ones first.
     */
    static QList<Exception> exceptions(const KSharedConfigPtr& scheme);

    /*!
     * Replaces the exceptions with \a exceptions (default ones: only their Enabled).
     */
    static void writeExceptions(const KSharedConfigPtr& scheme, const QList<Exception>& exceptions);

    /*!
     * Every [ToolsArea] entry as text, to tell whether anything changed.
     */
    static QString snapshot(const KSharedConfigPtr& scheme);

    /*!
     * Once: when [ToolsArea] has no Enabled key yet, moves the Silver theme's earlier
     * titlebar transparency settings and exceptions from silver/silverrc here and
     * deletes them there. Returns whether it did.
     */
    static bool migrateFromSilver(const KSharedConfigPtr& scheme);

    /*!
     * Follows the window decoration: one that doesn't draw a translucent tools area
     * (\a supported false) turns the tools area off, recording that it did
     * (\c DisabledByDecoration); a supporting one turns a tools area turned off that way
     * back on. Returns whether it changed anything.
     */
    static bool followDecoration(const KSharedConfigPtr& scheme, bool supported);

    /*!
     * The opacity of menus, 0–100 (100: opaque), from [Menus] \c Opacity. Widget styles
     * that draw see-through menus follow it, whichever style is in use.
     */
    static int menuOpacity(const KSharedConfigPtr& scheme);

    /*!
     * Writes \a opacity as [Menus] \c Opacity, notifying watchers.
     */
    static void writeMenuOpacity(const KSharedConfigPtr& scheme, int opacity);

    /*!
     * Once: when [Menus] has no \c Opacity key yet, moves the Silver style's earlier menu
     * opacity (silver/silverrc), or else the Breeze style's (breezerc), here and deletes it
     * from both. Returns whether it wrote anything.
     */
    static bool migrateMenuOpacity(const KSharedConfigPtr& scheme);
};

#endif
