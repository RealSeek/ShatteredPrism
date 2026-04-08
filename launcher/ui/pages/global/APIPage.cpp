// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (c) 2022 Lenny McLennington <lenny@sneed.church>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "APIPage.h"
#include "ui_APIPage.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTabBar>
#include <QValidator>
#include <QVariant>

#include "Application.h"
#include "BuildConfig.h"
#include "net/PasteUpload.h"
#include "settings/SettingsObject.h"
#include "tools/BaseProfiler.h"
#include "ui/GuiUtil.h"

namespace {
static const QString BMCLAPI_ROOT = QStringLiteral("https://bmclapi2.bangbang93.com/");
static const QString BMCLAPI_ASSETS = QStringLiteral("https://bmclapi2.bangbang93.com/assets/");
static const QString BMCLAPI_MAVEN = QStringLiteral("https://bmclapi2.bangbang93.com/maven/");
}  // namespace

APIPage::APIPage(QWidget* parent) : QWidget(parent), ui(new Ui::APIPage)
{
    // This is here so you can reorder the entries in the combobox without messing stuff up
    int comboBoxEntries[] = { PasteUpload::PasteType::Mclogs, PasteUpload::PasteType::NullPointer, PasteUpload::PasteType::PasteGG,
                              PasteUpload::PasteType::Hastebin };

    static const QRegularExpression s_validUrlRegExp("https?://.+");
    static const QRegularExpression s_validMSAClientID(
        QRegularExpression::anchoredPattern(".+"));

    ui->setupUi(this);

    for (auto pasteType : comboBoxEntries) {
        ui->pasteTypeComboBox->addItem(PasteUpload::PasteTypes.at(pasteType).name, pasteType);
    }

    void (QComboBox::*currentIndexChangedSignal)(int)(&QComboBox::currentIndexChanged);
    connect(ui->pasteTypeComboBox, currentIndexChangedSignal, this, &APIPage::updateBaseURLPlaceholder);
    // This function needs to be called even when the ComboBox's index is still in its default state.
    updateBaseURLPlaceholder(ui->pasteTypeComboBox->currentIndex());
    // NOTE: this allows http://, but we replace that with https later anyway
    ui->metaURL->setValidator(new QRegularExpressionValidator(s_validUrlRegExp, ui->metaURL));
    ui->resourceURL->setValidator(new QRegularExpressionValidator(s_validUrlRegExp, ui->resourceURL));
    ui->baseURLEntry->setValidator(new QRegularExpressionValidator(s_validUrlRegExp, ui->baseURLEntry));
    ui->legacyFMLLibsURL->setValidator(new QRegularExpressionValidator(s_validUrlRegExp, ui->legacyFMLLibsURL));
    ui->msaClientID->setValidator(new QRegularExpressionValidator(s_validMSAClientID, ui->msaClientID));

    ui->metaURL->setPlaceholderText(BuildConfig.META_URL);
    ui->resourceURL->setPlaceholderText(BuildConfig.DEFAULT_RESOURCE_BASE);
    ui->libraryURL->setPlaceholderText(BuildConfig.LIBRARY_BASE);
    ui->legacyFMLLibsURL->setPlaceholderText(BuildConfig.FMLLIBS_BASE_URL);
    ui->userAgentLineEdit->setPlaceholderText(BuildConfig.USER_AGENT);

    // Download Mirror preset
    ui->mirrorPreset->addItem(tr("Default"), QStringLiteral("default"));
    ui->mirrorPreset->addItem(tr("BMCLAPI (China)"), QStringLiteral("bmclapi"));
    ui->mirrorPreset->addItem(tr("Custom"), QStringLiteral("custom"));

    connect(ui->mirrorPreset, qOverload<int>(&QComboBox::currentIndexChanged), this, &APIPage::onMirrorPresetChanged);
    // Auto-switch to Custom when user manually edits URL fields
    connect(ui->resourceURL, &QLineEdit::textEdited, this, &APIPage::onMirrorUrlEdited);
    connect(ui->libraryURL, &QLineEdit::textEdited, this, &APIPage::onMirrorUrlEdited);
    connect(ui->legacyFMLLibsURL, &QLineEdit::textEdited, this, &APIPage::onMirrorUrlEdited);
    // Validate library URL
    ui->libraryURL->setValidator(new QRegularExpressionValidator(s_validUrlRegExp, ui->libraryURL));

    loadSettings();

    resetBaseURLNote();
    connect(ui->pasteTypeComboBox, currentIndexChangedSignal, this, &APIPage::updateBaseURLNote);
    connect(ui->baseURLEntry, &QLineEdit::textEdited, this, &APIPage::resetBaseURLNote);

    connect(ui->fetchKeyButton, &QPushButton::clicked, this, &APIPage::fetchKeyButtonPressed);
}

APIPage::~APIPage()
{
    delete ui;
}

void APIPage::resetBaseURLNote()
{
    ui->baseURLNote->hide();
    baseURLPasteType = ui->pasteTypeComboBox->currentIndex();
}

void APIPage::updateBaseURLNote(int index)
{
    if (baseURLPasteType == index) {
        ui->baseURLNote->hide();
    } else if (!ui->baseURLEntry->text().isEmpty()) {
        ui->baseURLNote->show();
    }
}

void APIPage::updateBaseURLPlaceholder(int index)
{
    int pasteType = ui->pasteTypeComboBox->itemData(index).toInt();
    QString pasteDefaultURL = PasteUpload::PasteTypes.at(pasteType).defaultBase;
    ui->baseURLEntry->setPlaceholderText(pasteDefaultURL);
}

void APIPage::loadSettings()
{
    auto s = APPLICATION->settings();

    int pasteType = s->get("PastebinType").toInt();
    QString pastebinURL = s->get("PastebinCustomAPIBase").toString();

    ui->baseURLEntry->setText(pastebinURL);
    int pasteTypeIndex = ui->pasteTypeComboBox->findData(pasteType);
    if (pasteTypeIndex == -1) {
        pasteTypeIndex = ui->pasteTypeComboBox->findData(PasteUpload::PasteType::Mclogs);
        ui->baseURLEntry->clear();
    }

    ui->pasteTypeComboBox->setCurrentIndex(pasteTypeIndex);

    if (bool fallbackMRBlockedMods = s->get("FallbackMRBlockedMods").toBool()) {
        ui->FallbackMRBlockedMods->setChecked(fallbackMRBlockedMods);
    }

    QString msaClientID = s->get("MSAClientIDOverride").toString();
    ui->msaClientID->setText(msaClientID);
    QString metaURL = s->get("MetaURLOverride").toString();
    ui->metaURL->setText(metaURL);
    QString resourceURL = s->get("ResourceURLOverride").toString();
    ui->resourceURL->setText(resourceURL);
    QString libraryURL = s->get("LibraryURLOverride").toString();
    ui->libraryURL->setText(libraryURL);
    QString fmlLibsURL = s->get("LegacyFMLLibsURLOverride").toString();
    ui->legacyFMLLibsURL->setText(fmlLibsURL);
    QString flameKey = s->get("FlameKeyOverride").toString();
    ui->flameKey->setText(flameKey);
    QString modrinthToken = s->get("ModrinthToken").toString();
    ui->modrinthToken->setText(modrinthToken);
    QString customUserAgent = s->get("UserAgentOverride").toString();
    ui->userAgentLineEdit->setText(customUserAgent);
    ui->technicClientID->setText(s->get("TechnicClientID").toString());

    // Sync mirror preset combo from current URL values
    syncMirrorPresetFromUrls();
}

void APIPage::applySettings()
{
    auto s = APPLICATION->settings();

    s->set("PastebinType", ui->pasteTypeComboBox->currentData().toInt());
    s->set("PastebinCustomAPIBase", ui->baseURLEntry->text());

    QString msaClientID = ui->msaClientID->text();
    s->set("MSAClientIDOverride", msaClientID);
    QUrl metaURL(ui->metaURL->text());
    QUrl resourceURL(ui->resourceURL->text());
    QUrl libraryURL(ui->libraryURL->text());
    QUrl fmlLibsURL(ui->legacyFMLLibsURL->text());

    auto addRequiredTrailingSlash = [](QUrl& url) {
        if (!url.isEmpty() && !url.path().endsWith('/')) {
            QString path = url.path();
            path.append('/');
            url.setPath(path);
        }
    };
    addRequiredTrailingSlash(metaURL);
    addRequiredTrailingSlash(resourceURL);
    addRequiredTrailingSlash(libraryURL);
    addRequiredTrailingSlash(fmlLibsURL);

    auto isLocalhost = [](const QUrl& url) { return url.host() == "localhost" || url.host() == "127.0.0.1" || url.host() == "::1"; };
    auto isUnsafe = [isLocalhost](const QUrl& url) { return !url.isEmpty() && url.scheme() == "http" && !isLocalhost(url); };
    auto upgradeToHTTPS = [isUnsafe](QUrl& url) {
        if (isUnsafe(url)) {
            url.setScheme("https");
        }
    };

    upgradeToHTTPS(metaURL);
    upgradeToHTTPS(resourceURL);
    upgradeToHTTPS(libraryURL);
    upgradeToHTTPS(fmlLibsURL);

    s->set("FallbackMRBlockedMods", ui->FallbackMRBlockedMods->checkState());
    s->set("MetaURLOverride", metaURL.toString());
    s->set("ResourceURLOverride", resourceURL.toString());
    s->set("LibraryURLOverride", libraryURL.toString());
    s->set("LegacyFMLLibsURLOverride", fmlLibsURL.toString());

    // Save MirrorRootURL based on current preset
    QString preset = ui->mirrorPreset->currentData().toString();
    if (preset == QStringLiteral("bmclapi")) {
        s->set("MirrorRootURL", BMCLAPI_ROOT);
    } else if (preset == QStringLiteral("default")) {
        s->set("MirrorRootURL", "");
    }
    // "custom" — keep MirrorRootURL as-is (user may have set it manually)

    QString flameKey = ui->flameKey->text();
    s->set("FlameKeyOverride", flameKey);
    QString modrinthToken = ui->modrinthToken->text();
    s->set("ModrinthToken", modrinthToken);
    s->set("UserAgentOverride", ui->userAgentLineEdit->text());
    s->set("TechnicClientID", ui->technicClientID->text());
}

void APIPage::fetchKeyButtonPressed()
{
    QString apiKey = GuiUtil::fetchFlameKey(parentWidget());

    if (!apiKey.isEmpty())
        ui->flameKey->setText(apiKey);
}

bool APIPage::apply()
{
    applySettings();
    return true;
}

void APIPage::retranslate()
{
    ui->retranslateUi(this);
}

void APIPage::onMirrorPresetChanged(int index)
{
    QString preset = ui->mirrorPreset->itemData(index).toString();
    if (preset == QStringLiteral("bmclapi")) {
        QSignalBlocker b1(ui->resourceURL);
        QSignalBlocker b2(ui->libraryURL);
        QSignalBlocker b3(ui->legacyFMLLibsURL);
        ui->resourceURL->setText(BMCLAPI_ASSETS);
        ui->libraryURL->setText(BMCLAPI_MAVEN);
        ui->legacyFMLLibsURL->setText(BMCLAPI_MAVEN);
    } else if (preset == QStringLiteral("default")) {
        QSignalBlocker b1(ui->resourceURL);
        QSignalBlocker b2(ui->libraryURL);
        QSignalBlocker b3(ui->legacyFMLLibsURL);
        ui->resourceURL->clear();
        ui->libraryURL->clear();
        ui->legacyFMLLibsURL->clear();
    }
    // "custom" — do nothing, let user edit freely
}

void APIPage::onMirrorUrlEdited()
{
    // When user manually edits any mirror-related URL, switch preset to Custom
    QSignalBlocker blocker(ui->mirrorPreset);
    int customIndex = ui->mirrorPreset->findData(QStringLiteral("custom"));
    if (customIndex != -1) {
        ui->mirrorPreset->setCurrentIndex(customIndex);
    }
}

void APIPage::syncMirrorPresetFromUrls()
{
    QSignalBlocker blocker(ui->mirrorPreset);

    QString resource = ui->resourceURL->text().trimmed();
    QString library = ui->libraryURL->text().trimmed();
    QString fmlLibs = ui->legacyFMLLibsURL->text().trimmed();

    bool isBmclapi = (resource == BMCLAPI_ASSETS) && (library == BMCLAPI_MAVEN) && (fmlLibs == BMCLAPI_MAVEN);
    bool isDefault = resource.isEmpty() && library.isEmpty() && fmlLibs.isEmpty();

    if (isBmclapi) {
        ui->mirrorPreset->setCurrentIndex(ui->mirrorPreset->findData(QStringLiteral("bmclapi")));
    } else if (isDefault) {
        ui->mirrorPreset->setCurrentIndex(ui->mirrorPreset->findData(QStringLiteral("default")));
    } else {
        ui->mirrorPreset->setCurrentIndex(ui->mirrorPreset->findData(QStringLiteral("custom")));
    }
}
