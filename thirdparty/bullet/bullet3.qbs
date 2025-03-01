import qbs

Project {
    id: bullet3
    property stringList srcFiles: [
        "src/LinearMath/*.cpp",
        "src/BulletCollision/CollisionDispatch/*.cpp",
        "src/BulletCollision/CollisionShapes/*.cpp",
        "src/BulletCollision/BroadphaseCollision/*.cpp",
        "src/BulletCollision/NarrowPhaseCollision/*.cpp",
        "src/BulletDynamics/Dynamics/*.cpp",
        "src/BulletDynamics/ConstraintSolver/*.cpp",
        "src/LinearMath/*.h",
        "src/BulletCollision/CollisionDispatch/*.h",
        "src/BulletCollision/CollisionShapes/*.h",
        "src/BulletCollision/BroadphaseCollision/*.h",
        "src/BulletCollision/NarrowPhaseCollision/*.h",
        "src/BulletDynamics/Dynamics/*.h",
        "src/BulletDynamics/ConstraintSolver/*.h",
        "src/*.h"
    ]

    property stringList incPaths: [
        "src"
    ]

    StaticLibrary {
        name: "bullet3"
        files: bullet3.srcFiles
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        bundle.isBundle: false

        cpp.defines: [ "BULLET_EXPORT" ]
        cpp.includePaths: bullet3.incPaths
        cpp.cxxLanguageVersion: "c++14"
        cpp.cxxStandardLibrary: "libc++"
        cpp.minimumMacosVersion: "10.12"
        cpp.minimumIosVersion: "10.0"
        cpp.minimumTvosVersion: "10.0"

		Properties {
            condition: qbs.targetOS.contains("darwin")
			cpp.commonCompilerFlags: "-Wno-argument-outside-range"
        }

        Properties {
            condition: qbs.targetOS.contains("android")
            Android.ndk.appStl: bullet3.ANDROID_STL
            Android.ndk.platform: bullet3.ANDROID
        }

        Group {
            name: "Install Static bullet3"
            fileTagsFilter: product.type
            qbs.install: true
            qbs.installDir: bullet3.SDK_PATH + "/" + qbs.targetOS[0] + "/" + qbs.architecture + "/static"
            qbs.installPrefix: bullet3.PREFIX
        }
    }
}
