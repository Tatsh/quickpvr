{
  uses_user_defaults: true,
  project_name: 'quickpvr',
  project_type: 'c++',
  version: '1.0.0',
  description: 'QuickLook previews and a converter for PowerVR and Sega Dreamcast PVR textures.',
  keywords: ['dreamcast', 'powervr', 'pvr', 'pvrtc', 'quicklook', 'texture'],
  license: 'BSD-3-Clause',
  security_policy_supported_versions: { '1.0.x': ':white_check_mark:' },
  want_codeql: false,
  want_tests: false,
  want_winget: false,
  clang_format_args: "$(git ls-files '*.cpp' '*.h' '*.mm' ':!:src/pvr/pvrtc.cpp')",
  clang_format+: {
    BreakInheritanceList: 'AfterColon',
    IncludeBlocks: 'Regroup',
    // Standard library, then windows.h, which other Windows headers need first, then other
    // lower-case system and third-party headers, then frameworks and Qt or KDE headers, then the
    // project's own headers.
    IncludeCategories: [
      {
        CaseSensitive: true,
        Priority: 1,
        Regex: '^<[a-z_]+>$',
      },
      {
        CaseSensitive: true,
        Priority: 2,
        Regex: '^<windows\\.h>$',
      },
      {
        CaseSensitive: true,
        Priority: 3,
        Regex: '^<[a-z]',
      },
      {
        CaseSensitive: true,
        Priority: 4,
        Regex: '^<[A-Z]',
      },
      {
        CaseSensitive: true,
        Priority: 5,
        Regex: '^"',
      },
    ],
    ReflowComments: true,
  },
  cz+: {
    commitizen+: {
      version_files+: ['man/pvr2png.1'],
    },
  },
  gitattributes+: ['*.pvp binary', '*.pvr binary', '*.PVR binary'],
  package_json+: {
    prettier+: {
      overrides+: [
        {
          files: ['*.plist', '*.plist.in'],
          options: {
            parser: 'xml',
          },
        },
      ],
    },
  },
  // codesign rejects entitlements with self-closing tags written as <true />.
  prettierignore+: [
    '*.c',
    '*.cmake',
    '*.cpp',
    '*.entitlements',
    '*.h',
    '*.mm',
    '*.pvp',
    '*.pvr',
    '*.PVR',
  ],
  shared_ignore+: ['/build-*/'],
  vcpkg+: {
    dependencies: [
      { name: 'argparse' },
      { name: 'spdlog' },
      { name: 'stb' },
    ],
  },
}
+ {
  want_scan_build: true,
  github+: { workflows+: {
    scan_build+: {
      // Ubuntu does not package KDE Frameworks 6, so the KIO thumbnailer is not analysed here.
      configure_command: 'cmake -S . -B build-scan -DCMAKE_BUILD_TYPE=Debug ' +
                         '-DQUICKPVR_BUILD_KDE_THUMBNAILER=OFF',
      documentation_steps: [
        {
          name: 'Install dependencies',
          run: |||
            sudo apt-get update
            sudo apt-get install -y doxygen graphviz
          |||,
        },
        {
          name: 'Configure',
          run: 'cmake -B "$GITHUB_WORKSPACE/build" -DBUILD_DOCS_ONLY=ON',
        },
        {
          name: 'Build documentation',
          run: 'cmake --build "$GITHUB_WORKSPACE/build" --target docs',
        },
      ],
      env: {},
      setup_steps: [
        {
          name: 'Install build dependencies',
          run: |||
            sudo apt-get update
            sudo apt-get install -y qt6-base-dev
          |||,
        },
        {
          name: 'Install current CMake',
          run: 'pipx install cmake',
        },
      ],
    },
  } },
}
+ { static_analysis_paths: ['src', 'tests'] }
