#ifndef ANIMCONVERTER_H
#define ANIMCONVERTER_H

#include <editor/assetconverter.h>
#include <resources/animationclip.h>

class AnimImportSettings : public AssetConverterSettings {
public:
    AnimImportSettings();

    bool isReadOnly() const override;
};

class AnimConverter : public AssetConverter {
    QStringList suffixes() const Q_DECL_OVERRIDE { return {"anim"}; }

    uint8_t convertFile(AssetConverterSettings *s) Q_DECL_OVERRIDE;
    AssetConverterSettings *createSettings() const Q_DECL_OVERRIDE;

    QString templatePath() const Q_DECL_OVERRIDE { return ":/Templates/Animation.anim"; }

private:
    Variant readJson(const string &data, AssetConverterSettings *settings);
    void toVersion1(Variant &variant);
};

#endif // ANIMCONVERTER_H
