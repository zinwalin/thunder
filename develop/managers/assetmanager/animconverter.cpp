#include "animconverter.h"

#include <QFile>

#include <json.h>
#include <bson.h>

#define TRACKS  "Tracks"

#define FORMAT_VERSION 2

AnimImportSettings::AnimImportSettings() {
    setType(MetaType::type<AnimationClip *>());
}

bool AnimImportSettings::isReadOnly() const {
    return false;
}

uint8_t AnimConverter::convertFile(AssetConverterSettings *settings) {
    QFile src(settings->source());
    if(src.open(QIODevice::ReadOnly)) {
        AnimationClip clip;
        VariantMap map;
        map[TRACKS] = readJson(src.readAll().toStdString(), settings).toList();
        clip.loadUserData(map);
        src.close();

        QFile file(settings->absoluteDestination());
        if(file.open(QIODevice::WriteOnly)) {
            ByteArray data = Bson::save( Engine::toVariant(&clip) );
            file.write(reinterpret_cast<const char *>(&data[0]), data.size());
            file.close();
            return 0;
        }
    }

    return 1;
}

AssetConverterSettings *AnimConverter::createSettings() const {
    return new AnimImportSettings();
}

Variant AnimConverter::readJson(const string &data, AssetConverterSettings *settings) {
    Variant result = Json::load(data);

    bool update = false;
    switch(settings->currentVersion()) {
        case 0: toVersion1(result); update = true;
        default: break;
    }

    if(update) {
        QFile src(settings->source());
        if(src.open(QIODevice::WriteOnly)) {
            settings->setCurrentVersion(settings->version());

            string data = Json::save(result, 0);
            src.write(data.c_str(), data.size());
            src.close();
        }
    }

    return result;
}

void AnimConverter::toVersion1(Variant &variant) {
    PROFILE_FUNCTION();

    // Create all declared objects
    VariantList &objects = *(reinterpret_cast<VariantList *>(variant.data()));
    for(auto &it : objects) {
        VariantList &o = *(reinterpret_cast<VariantList *>(it.data()));
        if(o.size() >= 5) {
            auto i = o.begin();
            ++i;
            ++i;
            ++i;
            ++i;

            // Load base properties
            VariantMap &properties = *(reinterpret_cast<VariantMap *>((*i).data()));
            VariantMap propertiesNew;
            for(auto &prop : properties) {
                QString property(prop.first.c_str());

                property.replace("_Rotation", "quaternion");
                property.replace(0, 1, property[0].toLower());

                propertiesNew[property.toStdString()] = prop.second;
            }
            properties = propertiesNew;
        }
    }
}
