// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QString>
#include <QUrl>
#include <utility>
#include <vector>

/**
 * @brief Utility functions for rewriting download URLs to use a mirror.
 *
 * When a mirror root URL is configured (e.g. BMCLAPI), these functions
 * rewrite known official Minecraft/Forge/Fabric/NeoForge URLs to use
 * the mirror with appropriate sub-path mappings.
 */
namespace MirrorUtils {

/**
 * @brief Known URL prefix mappings for BMCLAPI-style mirrors.
 *
 * Each entry maps an official URL prefix to a sub-path under the mirror root.
 * Empty suffix means direct mapping to the mirror root.
 */
inline const std::vector<std::pair<QString, QString>>& knownPrefixes()
{
    static const std::vector<std::pair<QString, QString>> prefixes = {
        // Mojang domains — map directly to mirror root
        { QStringLiteral("https://piston-data.mojang.com/"), QString() },
        { QStringLiteral("https://piston-meta.mojang.com/"), QString() },
        { QStringLiteral("https://launchermeta.mojang.com/"), QString() },
        { QStringLiteral("https://launcher.mojang.com/"), QString() },
        // Minecraft libraries
        { QStringLiteral("https://libraries.minecraft.net/"), QStringLiteral("libraries/") },
        // Forge / NeoForge / Fabric maven
        { QStringLiteral("https://maven.minecraftforge.net/"), QStringLiteral("maven/") },
        { QStringLiteral("https://files.minecraftforge.net/maven/"), QStringLiteral("maven/") },
        { QStringLiteral("http://files.minecraftforge.net/maven/"), QStringLiteral("maven/") },
        { QStringLiteral("https://maven.neoforged.net/releases/"), QStringLiteral("maven/") },
        { QStringLiteral("https://maven.fabricmc.net/"), QStringLiteral("maven/") },
        // Maven Central
        { QStringLiteral("https://repo1.maven.org/maven2/"), QStringLiteral("maven/") },
        { QStringLiteral("https://repo.maven.apache.org/maven2/"), QStringLiteral("maven/") },
        // Authlib-Injector
        { QStringLiteral("https://authlib-injector.yushi.moe/"), QStringLiteral("mirrors/authlib-injector/") },
    };
    return prefixes;
}

/**
 * @brief Normalize a base URL to ensure it ends with a trailing slash.
 */
inline QString normalizeUrl(const QString& url)
{
    QString result = url.trimmed();
    if (!result.isEmpty() && !result.endsWith('/')) {
        result += '/';
    }
    return result;
}

/**
 * @brief Rewrite a URL using the mirror root and known prefix mappings.
 *
 * @param url The original URL string.
 * @param mirrorRoot The mirror root URL (e.g. "https://bmclapi2.bangbang93.com/").
 * @return The rewritten URL, or the original if no prefix matched.
 */
inline QString rewriteUrl(const QString& url, const QString& mirrorRoot)
{
    if (mirrorRoot.isEmpty()) {
        return url;
    }

    const QString root = normalizeUrl(mirrorRoot);
    for (const auto& [prefix, suffix] : knownPrefixes()) {
        if (url.startsWith(prefix)) {
            return root + suffix + url.mid(prefix.size());
        }
    }
    return url;
}

/**
 * @brief Rewrite a QUrl using the mirror root.
 */
inline QUrl rewriteUrl(const QUrl& url, const QString& mirrorRoot)
{
    if (mirrorRoot.isEmpty()) {
        return url;
    }
    return QUrl(rewriteUrl(url.toString(), mirrorRoot));
}

/**
 * @brief Known mod platform URL prefix mappings for mcimirror.top fallback.
 *
 * These are used as fallback mirrors for mod platform CDN downloads
 * when a mirror root is configured (indicating the user wants China mirrors).
 */
inline const std::vector<std::pair<QString, QString>>& modPlatformPrefixes()
{
    static const std::vector<std::pair<QString, QString>> prefixes = {
        { QStringLiteral("https://api.modrinth.com"), QStringLiteral("https://mod.mcimirror.top/modrinth") },
        { QStringLiteral("https://cdn.modrinth.com"), QStringLiteral("https://mod.mcimirror.top") },
        { QStringLiteral("https://api.curseforge.com"), QStringLiteral("https://mod.mcimirror.top/curseforge") },
        { QStringLiteral("https://edge.forgecdn.net"), QStringLiteral("https://mod.mcimirror.top") },
        { QStringLiteral("https://mediafilez.forgecdn.net"), QStringLiteral("https://mod.mcimirror.top") },
    };
    return prefixes;
}

/**
 * @brief Get a fallback mirror URL for mod platform downloads.
 *
 * @param url The original mod download URL.
 * @param mirrorRoot If non-empty, indicates mirror mode is active.
 * @return The mirror URL, or empty string if no mapping matched or mirror is disabled.
 */
inline QString modFallbackUrl(const QString& url, const QString& mirrorRoot)
{
    if (mirrorRoot.isEmpty()) {
        return {};
    }

    for (const auto& [prefix, replacement] : modPlatformPrefixes()) {
        if (url.startsWith(prefix)) {
            return replacement + url.mid(prefix.size());
        }
    }
    return {};
}

inline QUrl modFallbackUrl(const QUrl& url, const QString& mirrorRoot)
{
    QString result = modFallbackUrl(url.toString(), mirrorRoot);
    return result.isEmpty() ? QUrl() : QUrl(result);
}

}  // namespace MirrorUtils
