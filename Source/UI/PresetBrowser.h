// ==========================================
// UI/PresetBrowser.h   （V1.1.0 新規 / LIFT-X の PresetBrowser を移植）
//
//  3 カラムのプリセットブラウザ
//   ・左  : カテゴリ（All / Factory / User / Favorites）
//   ・中央: サブカテゴリ（Factory の分類、または保存時のフォルダ名）
//   ・右  : 検索ボックス ＋ 一覧（★ でお気に入り／1 クリックで即読込）
//   ・下  : 名前入力 ＋ サブカテゴリ入力 ＋ Save / Init / Close
//
//  ユーザープリセットは presetDir 以下の *.xml。サブカテゴリ＝サブフォルダ名。
//  お気に入りは presetDir/_favorites.txt に 1 行 1 件で保存する。
//  Factory は "factory::<index>" という ID で同じファイルに混ぜて記録する。
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "ColorPalette.h"

class PresetBrowser : public juce::Component
{
public:
    std::function<void(const juce::File&)> onLoad;        // ユーザープリセット読込
    std::function<void()> onInit;                          // Init（既定値へ）
    std::function<void(const juce::String& name, const juce::String& subCat)> onSave;
    std::function<void()> onClose;
    std::function<void(int)> onLoadFactory;                // Factory を index で読込

    struct FactoryItem { juce::String name, category; int index = 0; };
    void setFactoryPresets(const juce::Array<FactoryItem>& items) { factoryAll = items; refresh(); }

    explicit PresetBrowser(juce::File presetDir) : rootDir(presetDir)
    {
        catModel.owner = this; subModel.owner = this; fileModel.owner = this;
        categories = { "All", "Factory", "User", "Favorites" };

        catList.setModel(&catModel); subList.setModel(&subModel); fileList.setModel(&fileModel);
        catList.setRowHeight(34); subList.setRowHeight(28); fileList.setRowHeight(26);
        catList.setColour(juce::ListBox::backgroundColourId, QMColors::well);
        subList.setColour(juce::ListBox::backgroundColourId, QMColors::well.brighter(0.03f));
        fileList.setColour(juce::ListBox::backgroundColourId, QMColors::well.brighter(0.06f));
        for (auto* l : { &catList, &subList, &fileList }) addAndMakeVisible(*l);

        auto styleEdit = [](juce::TextEditor& e, const juce::String& hint)
        {
            e.setColour(juce::TextEditor::backgroundColourId, QMColors::well);
            e.setColour(juce::TextEditor::textColourId, QMColors::text);
            e.setColour(juce::TextEditor::outlineColourId, QMColors::panelLine);
            e.setTextToShowWhenEmpty(hint, QMColors::textDim);
        };
        addAndMakeVisible(searchBox); styleEdit(searchBox, "Search...");
        searchBox.onTextChange = [this] { updateFiles(); };
        addAndMakeVisible(nameBox);   styleEdit(nameBox, "Preset name");
        addAndMakeVisible(subBox);    styleEdit(subBox, "Subcategory (optional)");

        auto styleBtn = [](juce::TextButton& b, juce::Colour on)
        {
            b.setColour(juce::TextButton::buttonColourId, QMColors::track);
            b.setColour(juce::TextButton::buttonOnColourId, on);
            b.setColour(juce::TextButton::textColourOffId, QMColors::text);
        };
        addAndMakeVisible(saveBtn);  saveBtn.setButtonText("Save");   styleBtn(saveBtn, QMColors::accentMorph);
        addAndMakeVisible(initBtn);  initBtn.setButtonText("Init");   styleBtn(initBtn, QMColors::accentFilter);
        addAndMakeVisible(closeBtn); closeBtn.setButtonText("Close"); styleBtn(closeBtn, QMColors::rose);
        saveBtn.onClick = [this] { doSave(); };
        initBtn.onClick = [this] { if (onInit) onInit(); };
        closeBtn.onClick = [this] { if (onClose) onClose(); };

        refresh();
    }

    void refresh() { scan(); updateSubCategories(); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(QMColors::panel);
        g.setColour(QMColors::panelLine);
        g.drawRect(getLocalBounds(), 1);

        g.setColour(QMColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("CATEGORY", colX(0) + 8, 4, 120, 14, juce::Justification::left);
        g.drawText("SUBCATEGORY", colX(1) + 8, 4, 160, 14, juce::Justification::left);
        g.drawText("PRESETS", colX(2) + 8, 4, 120, 14, juce::Justification::left);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(4);
        area.removeFromTop(18);
        auto bottom = area.removeFromBottom(30);
        area.removeFromBottom(4);

        closeBtn.setBounds(bottom.removeFromRight(62).reduced(2));
        initBtn.setBounds(bottom.removeFromRight(62).reduced(2));
        saveBtn.setBounds(bottom.removeFromRight(62).reduced(2));
        subBox.setBounds(bottom.removeFromRight(160).reduced(2));
        nameBox.setBounds(bottom.reduced(2));

        const int w = area.getWidth() / 5;
        catList.setBounds(area.removeFromLeft(w).reduced(2));
        subList.setBounds(area.removeFromLeft(w + w / 2).reduced(2));
        searchBox.setBounds(area.removeFromTop(26).reduced(2));
        fileList.setBounds(area.reduced(2));
    }

    /** 外部（エディタ）から現在のプリセットを設定してハイライトを同期する */
    void setCurrentFile(const juce::File& f)
    {
        currentFile = f; currentFactoryIndex = -1; revealCurrent();
    }
    void setCurrentFactory(int idx)
    {
        currentFactoryIndex = idx; currentFile = juce::File(); revealCurrent();
    }

private:
    int colX(int i) const
    {
        const int w = (getWidth() - 8) / 5;
        return i == 0 ? 4 : i == 1 ? 4 + w : 4 + w + w + w / 2;
    }

    juce::File rootDir;
    juce::ListBox catList{ "cat", nullptr }, subList{ "sub", nullptr }, fileList{ "file", nullptr };
    juce::TextEditor searchBox, nameBox, subBox;
    juce::TextButton saveBtn, initBtn, closeBtn;

    juce::StringArray categories, subCategories, favorites, pendingRemovals;
    int selCat = 0, selSub = 0;
    juce::File currentFile;
    int currentFactoryIndex = -1;

    struct PItem { juce::File file; juce::String name; juce::String subCat; };
    juce::Array<PItem> allPresets, currentList;
    juce::Array<FactoryItem> factoryAll, factoryCur;
    bool allMode = false;

    bool isFactory() const { return selCat >= 0 && selCat < categories.size() && categories[selCat] == "Factory"; }
    bool showsFactory() const { return isFactory() || allMode; }
    int  factoryRowCount() const { return factoryCur.size(); }

    // ---------------- お気に入り ----------------
    // 複数インスタンスが同時に書き込む可能性があるため、保存時に必ず
    // ファイルを読み直してマージする。単純な上書きだと他方の追加が消える。
    juce::File favFile() const { return rootDir.getChildFile("_favorites.txt"); }

    void loadFavorites()
    {
        favorites.clear();
        auto f = favFile();
        if (f.existsAsFile()) { favorites.addLines(f.loadFileAsString()); favorites.removeEmptyStrings(); }
    }

    void saveFavorites()
    {
        juce::StringArray latest;
        auto f = favFile();
        if (f.existsAsFile()) { latest.addLines(f.loadFileAsString()); latest.removeEmptyStrings(); }

        for (auto& rem : pendingRemovals) latest.removeString(rem);
        for (auto& fav : favorites) if (!latest.contains(fav)) latest.add(fav);

        rootDir.createDirectory();
        f.replaceWithText(latest.joinIntoString("\n"), false, false, "\n");

        favorites = latest;
        pendingRemovals.clear();
    }

    juce::String factoryFavId(int index) const { return "factory::" + juce::String(index); }
    bool isFactoryFavorite(int index) const { return favorites.contains(factoryFavId(index)); }

    void toggleFavorite(const juce::String& id, int row)
    {
        if (favorites.contains(id)) { favorites.removeString(id); pendingRemovals.add(id); }
        else                          favorites.add(id);

        saveFavorites();

        if (categories[selCat] == "Favorites") updateFiles();
        else                                    fileList.repaintRow(row);
    }

    // ---------------- 走査・絞り込み ----------------
    void scan()
    {
        allPresets.clear();
        if (rootDir.isDirectory())
            for (auto& f : rootDir.findChildFiles(juce::File::findFiles, true, "*.xml"))
            {
                juce::String sub = "Uncategorized";
                auto parent = f.getParentDirectory();
                if (parent != rootDir) sub = parent.getFileName();
                allPresets.add({ f, f.getFileNameWithoutExtension(), sub });
            }
        loadFavorites();
    }

    void updateSubCategories()
    {
        subCategories.clear();
        subCategories.add("All");

        const juce::String sc = categories[selCat];

        if (isFactory() || sc == "All" || sc == "Favorites")
            for (auto& it : factoryAll)
            {
                if (sc == "Favorites" && !isFactoryFavorite(it.index)) continue;
                if (!subCategories.contains(it.category)) subCategories.add(it.category);
            }

        if (!isFactory())
            for (auto& p : allPresets)
            {
                if (sc == "Favorites" && !favorites.contains(p.file.getFullPathName())) continue;
                if (!subCategories.contains(p.subCat)) subCategories.add(p.subCat);
            }

        selSub = 0;
        subList.updateContent();
        updateFiles();
    }

    void updateFiles()
    {
        currentList.clear();
        factoryCur.clear();

        const juce::String c = categories[selCat];
        allMode = (c == "All");
        const juce::String s = (selSub >= 0 && selSub < subCategories.size()) ? subCategories[selSub] : "All";
        const juce::String q = searchBox.getText().trim().toLowerCase();
        auto match = [&q](const juce::String& n) { return q.isEmpty() || n.toLowerCase().contains(q); };

        if (showsFactory() || c == "Favorites")
            for (auto& it : factoryAll)
            {
                if (c == "Favorites" && !isFactoryFavorite(it.index)) continue;
                if (s != "All" && it.category != s) continue;
                if (!match(it.name)) continue;
                factoryCur.add(it);
            }

        if (!isFactory())
            for (auto& p : allPresets)
            {
                if (c == "Favorites" && !favorites.contains(p.file.getFullPathName())) continue;
                if (s != "All" && p.subCat != s) continue;
                if (!match(p.name)) continue;
                currentList.add(p);
            }

        fileList.updateContent();
        fileList.repaint();
    }

    /** 現在のプリセットが絞り込み結果に含まれていれば、その行までスクロールする */
    void revealCurrent()
    {
        const int fc = factoryRowCount();
        int row = -1;

        if (currentFactoryIndex >= 0)
        {
            for (int i = 0; i < fc; ++i)
                if (factoryCur.getReference(i).index == currentFactoryIndex) { row = i; break; }
        }
        else if (currentFile != juce::File())
        {
            for (int i = 0; i < currentList.size(); ++i)
                if (currentList.getReference(i).file == currentFile) { row = fc + i; break; }
        }

        if (row >= 0) fileList.scrollToEnsureRowIsOnscreen(row);
        fileList.repaint();
    }

    void doSave()
    {
        if (!onSave) return;
        juce::String name = nameBox.getText().trim();
        if (name.isEmpty()) name = "Preset";
        onSave(name, subBox.getText().trim());
        refresh();
    }

    void showItemMenu(juce::File file)
    {
        juce::PopupMenu m;
        m.addItem(1, "Delete \"" + file.getFileNameWithoutExtension() + "\"");
        m.showMenuAsync(juce::PopupMenu::Options(), [this, file](int result)
        {
            if (result != 1) return;

            juce::NativeMessageBox::showYesNoBox(juce::MessageBoxIconType::WarningIcon,
                "Delete Preset",
                "Delete \"" + file.getFileNameWithoutExtension() + "\" permanently?",
                nullptr,
                juce::ModalCallbackFunction::create([this, file](int yes)
                {
                    if (yes != 1) return;
                    const auto id = file.getFullPathName();
                    if (favorites.contains(id)) { favorites.removeString(id); saveFavorites(); }
                    if (currentFile == file) currentFile = juce::File();
                    file.deleteFile();
                    refresh();
                }));
        });
    }

    // ---------------- ListBox モデル ----------------
    static juce::String starFilled() { return juce::String::fromUTF8("\xE2\x98\x85"); }
    static juce::String starHollow() { return juce::String::fromUTF8("\xE2\x98\x86"); }

    struct CatM : juce::ListBoxModel
    {
        PresetBrowser* owner = nullptr;
        int getNumRows() override { return owner ? owner->categories.size() : 0; }
        void paintListBoxItem(int r, juce::Graphics& g, int w, int h, bool) override
        {
            if (owner == nullptr) return;
            const bool a = (r == owner->selCat);
            if (a) g.fillAll(QMColors::accentFilter.withAlpha(0.18f));
            g.setColour(a ? QMColors::text : QMColors::textDim);
            g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
            g.drawText(owner->categories[r], 12, 0, w - 20, h, juce::Justification::centredLeft);
        }
        void listBoxItemClicked(int r, const juce::MouseEvent&) override
        {
            if (owner != nullptr) { owner->selCat = r; owner->updateSubCategories(); }
        }
    } catModel;

    struct SubM : juce::ListBoxModel
    {
        PresetBrowser* owner = nullptr;
        int getNumRows() override { return owner ? owner->subCategories.size() : 0; }
        void paintListBoxItem(int r, juce::Graphics& g, int w, int h, bool) override
        {
            if (owner == nullptr) return;
            const bool a = (r == owner->selSub);
            if (a) g.fillAll(QMColors::accentFilter.withAlpha(0.14f));
            g.setColour(a ? QMColors::text : QMColors::textDim);
            g.setFont(juce::Font(juce::FontOptions(12.5f)));
            g.drawText(owner->subCategories[r], 12, 0, w - 20, h, juce::Justification::centredLeft);
        }
        void listBoxItemClicked(int r, const juce::MouseEvent&) override
        {
            if (owner != nullptr) { owner->selSub = r; owner->updateFiles(); }
        }
    } subModel;

    struct FileM : juce::ListBoxModel
    {
        PresetBrowser* owner = nullptr;

        int getNumRows() override
        {
            return owner ? owner->factoryRowCount() + owner->currentList.size() : 0;
        }

        void paintListBoxItem(int r, juce::Graphics& g, int w, int h, bool) override
        {
            if (owner == nullptr) return;
            const int fc = owner->factoryRowCount();

            auto drawStar = [&g, h](bool fav)
            {
                g.setColour(fav ? QMColors::peach : QMColors::textDim.withAlpha(0.45f));
                g.setFont(juce::Font(juce::FontOptions(15.0f)));
                g.drawText(fav ? starFilled() : starHollow(), 6, 0, 20, h, juce::Justification::centred);
            };

            auto drawSelection = [&g, h](bool sel)
            {
                if (!sel) return;
                g.fillAll(QMColors::accentFilter.withAlpha(0.16f));
                g.setColour(QMColors::accentFilter);
                g.fillRect(0, 0, 3, h);
            };

            if (r < fc)   // ---- Factory 行 ----
            {
                auto& fi = owner->factoryCur.getReference(r);
                const bool sel = (fi.index == owner->currentFactoryIndex);
                drawSelection(sel);
                drawStar(owner->isFactoryFavorite(fi.index));

                g.setColour(sel ? QMColors::text : QMColors::textDim);
                g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
                g.drawText(fi.name + "    [" + fi.category + "]",
                           30, 0, w - 36, h, juce::Justification::centredLeft);
                return;
            }

            const int ui = r - fc;
            if (ui < 0 || ui >= owner->currentList.size()) return;

            auto& it = owner->currentList.getReference(ui);
            const bool sel = (it.file == owner->currentFile);
            drawSelection(sel);
            drawStar(owner->favorites.contains(it.file.getFullPathName()));

            g.setColour(sel ? QMColors::text : QMColors::textDim);
            g.setFont(juce::Font(juce::FontOptions(12.5f, sel ? juce::Font::bold : juce::Font::plain)));

            juce::String label = it.name;
            if (it.subCat.isNotEmpty() && it.subCat != "Uncategorized")
                label += "    [" + it.subCat + "]";

            g.drawText(label, 30, 0, w - 36, h, juce::Justification::centredLeft);
        }

        void listBoxItemClicked(int r, const juce::MouseEvent& e) override
        {
            if (owner == nullptr) return;
            const int fc = owner->factoryRowCount();

            if (r < fc)   // ---- Factory 行 ----
            {
                auto& fi = owner->factoryCur.getReference(r);

                if (e.x < 28)                       // ★ トグル
                {
                    owner->toggleFavorite(owner->factoryFavId(fi.index), r);
                }
                else                                // 1 クリックで即読込
                {
                    owner->currentFactoryIndex = fi.index;
                    owner->currentFile = juce::File();
                    if (owner->onLoadFactory) owner->onLoadFactory(fi.index);
                    owner->fileList.repaint();
                }
                return;
            }

            const int ui = r - fc;
            if (ui < 0 || ui >= owner->currentList.size()) return;
            auto& it = owner->currentList.getReference(ui);

            if (e.mods.isPopupMenu()) { owner->showItemMenu(it.file); return; }

            if (e.x < 28)
            {
                owner->toggleFavorite(it.file.getFullPathName(), r);
            }
            else
            {
                owner->currentFile = it.file;
                owner->currentFactoryIndex = -1;
                if (owner->onLoad) owner->onLoad(it.file);
                owner->fileList.repaint();
            }
        }
    } fileModel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
