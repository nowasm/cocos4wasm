const fs = require('fs-extra');
const path = require('path');
const chalk = require('chalk');

const TEMPLATES_DIR = path.join(__dirname, '../../../../templates');
const SUPPORTED_PLATFORMS = ['windows', 'android', 'ios', 'linux', 'mac', 'wasm32'];

async function newCommand(projectName, options) {
  const { platform, package, directory, engine, dataDir } = options;
  const packageName = package || 'com.cocos.helloworld';
  const projectDir = directory || '.';

  // Validate platform
  if (!SUPPORTED_PLATFORMS.includes(platform)) {
    console.error(chalk.red(`Error: Unsupported platform "${platform}". Supported platforms: ${SUPPORTED_PLATFORMS.join(', ')}`));
    process.exit(1);
  }

  // Resolve project path
  const projectPath = path.resolve(projectDir, projectName);

  // Check if directory already exists
  if (await fs.pathExists(projectPath)) {
    console.error(chalk.red(`Error: Directory "${projectPath}" already exists.`));
    process.exit(1);
  }

  console.log(chalk.blue(`Creating new Cocos Native project: ${projectName}`));
  console.log(chalk.blue(`Platform: ${platform}`));
  console.log(chalk.blue(`Output: ${projectPath}`));

  try {
    // Create project directory
    await fs.ensureDir(projectPath);

    // Copy platform-specific template
    const platformTemplateDir = path.join(TEMPLATES_DIR, platform);
    if (!await fs.pathExists(platformTemplateDir)) {
      console.error(chalk.red(`Error: wasm32 template directory not found: ${platformTemplateDir}`));
      process.exit(1);
    }
    await fs.copy(platformTemplateDir, path.join(projectPath, 'proj'), { preserveTimestamps: true });
    console.log(chalk.green(`✓ Copied ${platform} template`));

    // Copy cmake templates
    const cmakeDir = path.join(TEMPLATES_DIR, 'cmake');
    if (await fs.pathExists(cmakeDir)) {
      await fs.copy(cmakeDir, path.join(projectPath, 'cmake'), { preserveTimestamps: true });
      console.log(chalk.green('✓ Copied cmake templates'));
    }

    // Copy common templates
    const commonDir = path.join(TEMPLATES_DIR, 'common');
    if (await fs.pathExists(commonDir)) {
      await fs.copy(commonDir, path.join(projectPath, 'common'), { preserveTimestamps: true });
      console.log(chalk.green('✓ Copied common templates'));
    }

    // Handle platform-specific directories
    if (platform === 'android') {
      const androidTemplateDir = path.join(TEMPLATES_DIR, 'android/template');
      if (await fs.pathExists(androidTemplateDir)) {
        await fs.copy(androidTemplateDir, path.join(projectPath, 'native'), { preserveTimestamps: true });
        console.log(chalk.green('✓ Copied Android template'));
      }

      // Copy Android build files
      const androidBuildDir = path.join(TEMPLATES_DIR, 'android/build');
      if (await fs.pathExists(androidBuildDir)) {
        await fs.copy(androidBuildDir, path.join(projectPath, 'native/build'), { preserveTimestamps: true });
        console.log(chalk.green('✓ Copied Android build files'));
      }
    }

    // Create Classes directory
    await fs.ensureDir(path.join(projectPath, 'Classes'));

    // Create Resources directory
    await fs.ensureDir(path.join(projectPath, 'Resources'));

    // Create root CMakeLists.txt
    const enginePath = engine || null;
    await createRootCMakeLists(projectPath, projectName, platform, enginePath);

    // Write engine path config file for auto-detection
    const currentEnginePath = path.resolve(__dirname, '../../../../');
    await fs.writeFile(path.join(projectPath, '.cocos-engine'), currentEnginePath);
    console.log(chalk.green('✓ Created .cocos-engine config'));

    // Write data directory config if specified
    if (dataDir) {
      await fs.writeFile(path.join(projectPath, '.cocos-data-dir'), dataDir);
      console.log(chalk.green(`✓ Created .cocos-data-dir: ${dataDir}`));
    }

    // Replace variables in files
    await replaceVariables(projectPath, projectName, packageName, platform);

    console.log(chalk.green(`\n✓ Project "${projectName}" created successfully!`));
    console.log(chalk.blue(`\nNote: Place your project inside the c4 engine directory for automatic engine detection.`));
    console.log(chalk.blue(`For example: move the project to ${path.dirname(projectPath)}/`));
    console.log(chalk.blue(`\nNext steps:`));
    console.log(`  cd ${projectPath}`);
    console.log(`  cocos compile -p ${platform}`);

    if (platform === 'wasm32') {
      console.log('\nTo build your wasm32 project:');
      console.log('  1. Activate emsdk:  source /path/to/emsdk/emsdk_env.sh');
      console.log('  2. Configure:       emcmake cmake .. -G Ninja');
      console.log('  3. Build:           cmake --build .');
      console.log('  4. Preview:         python -m http.server 8080');
    }

  } catch (error) {
    console.error(chalk.red(`Error creating project: ${error.message}`));
    process.exit(1);
  }
}

async function createRootCMakeLists(projectPath, projectName, platform, enginePath) {
  // Auto-detect engine path using .cocos-engine config file or search parent directories
  let autoDetectConfig = '';
  if (!enginePath) {
    autoDetectConfig = `# Auto-detect engine root
# First check for .cocos-engine config file in project directory
set(COCOS_ROOT "")
if(EXISTS "\${CMAKE_CURRENT_SOURCE_DIR}/.cocos-engine")
    file(READ "\${CMAKE_CURRENT_SOURCE_DIR}/.cocos-engine" COCOS_ROOT)
    string(STRIP "\${COCOS_ROOT}" COCOS_ROOT)
    message(STATUS "Using Cocos engine from .cocos-engine: \${COCOS_ROOT}")
else()
    # Fallback: search parent directories
    get_filename_component(COCOS_ROOT "\${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
    foreach(i RANGE 10)
        if(EXISTS "\${COCOS_ROOT}/CMakeLists.txt" AND EXISTS "\${COCOS_ROOT}/cocos")
            message(STATUS "Detected Cocos engine at: \${COCOS_ROOT}")
            break()
        endif()
        get_filename_component(PARENT_DIR "\${COCOS_ROOT}/.." ABSOLUTE)
        if(PARENT_DIR STREQUAL COCOS_ROOT)
            message(FATAL_ERROR "Cocos engine not found. Please create .cocos-engine file with engine path.")
        endif()
        set(COCOS_ROOT "\${PARENT_DIR}")
    endforeach()
endif()`;
  }

  let cocosRootConfig;
  if (enginePath) {
    // User specified custom engine path
    cocosRootConfig = 'set(COCOS_ROOT "' + enginePath + '" CACHE PATH "Cocos Engine Root")';
  } else {
    cocosRootConfig = autoDetectConfig;
  }

  const cmakeContent = `cmake_minimum_required(VERSION 3.16)
project(${projectName})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Engine directory (c4)
${cocosRootConfig}

# Build options
option(USE_SPINE_3_8 "Use Spine 3.8" OFF)
option(USE_SPINE_4_2 "Use Spine 4.2" ON)
option(CC_DEBUG_FORCE "Enable debug mode" OFF)
option(USE_V8_DEBUGGER "Enable V8 debugger" OFF)

# Set RES_DIR and COCOS_X_PATH as CACHE variables
set(RES_DIR "\${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Resource directory")
set(COCOS_X_PATH "\${COCOS_ROOT}" CACHE PATH "Cocos engine path")

# Flag to prevent double engine loading
set(CC_ENGINE_INCLUDED TRUE CACHE BOOL "Engine already included")

# Load project cfg before engine (so platform options like USE_SE_QJS take effect)
if(EXISTS "\${CMAKE_CURRENT_SOURCE_DIR}/proj/cfg.cmake")
    include(\${CMAKE_CURRENT_SOURCE_DIR}/proj/cfg.cmake)
endif()

# Include Cocos engine (only once)
include(\${COCOS_ROOT}/CMakeLists.txt)

# Add project sources (skip engine re-include in common/)
set(CC_PROJECT_SKIP_ENGINE TRUE)
add_subdirectory(proj)
`;
  await fs.writeFile(path.join(projectPath, 'CMakeLists.txt'), cmakeContent);
  console.log(chalk.green('✓ Created root CMakeLists.txt'));
}

async function replaceVariables(projectPath, projectName, packageName, platform) {
  // For Android, derive the game package by replacing the last segment with "game"
  // e.g., com.cocos.helloworld -> com.cocos.game
  const parts = packageName.split('.');
  if (parts.length > 1) {
    parts[parts.length - 1] = 'game';
  }
  const androidGamePackage = parts.join('.');

  const replacements = [
    { pattern: /CocosGame/g, replacement: projectName },
    { pattern: /com\.cocos\.helloworld/g, replacement: packageName },
    { pattern: /com\.cocos\.game/g, replacement: androidGamePackage }
  ];

  async function walkDir(dir) {
    const entries = await fs.readdir(dir, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        await walkDir(fullPath);
      } else if (entry.isFile() && !entry.name.endsWith('.png') && !entry.name.endsWith('.jpg')) {
        // Skip binary files
        try {
          let content = await fs.readFile(fullPath, 'utf8');
          let modified = false;
          for (const { pattern, replacement } of replacements) {
            if (pattern.test(content)) {
              content = content.replace(pattern, replacement);
              modified = true;
            }
          }
          if (modified) {
            await fs.writeFile(fullPath, content);
          }
        } catch (e) {
          // Skip binary or unreadable files
        }
      }
    }
  }

  await walkDir(projectPath);
  console.log(chalk.green('✓ Replaced template variables'));
}

module.exports = newCommand;
