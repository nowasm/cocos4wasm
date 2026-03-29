const fs = require('fs-extra');
const path = require('path');
const { execSync } = require('child_process');
const chalk = require('chalk');

const SUPPORTED_PLATFORMS = ['windows', 'android', 'ios', 'linux', 'mac', 'wasm32'];

async function runCommand(projectPath, options) {
  const { platform, buildDir, dataDir, skipRun } = options;
  const projectName = path.basename(projectPath);

  // Check for .cocos-data-dir config file if not provided via CLI
  let defaultDataDir = dataDir;
  if (!defaultDataDir) {
    const dataDirConfig = path.join(projectPath, '.cocos-data-dir');
    if (await fs.pathExists(dataDirConfig)) {
      defaultDataDir = (await fs.readFile(dataDirConfig, 'utf8')).trim();
    }
  }

  // Validate platform
  if (!SUPPORTED_PLATFORMS.includes(platform)) {
    console.error(chalk.red(`Error: Unsupported platform "${platform}". Supported platforms: ${SUPPORTED_PLATFORMS.join(', ')}`));
    process.exit(1);
  }

  console.log(chalk.blue(`Building and running project: ${projectPath}`));
  console.log(chalk.blue(`Platform: ${platform}`));

  try {
    // Step 1: Build the project
    const buildPath = buildDir || path.join(projectPath, 'build');
    console.log(chalk.blue('\n[1/3] Running CMake configuration...'));

    // Create build directory if not exists
    await fs.ensureDir(buildPath);

    // Run CMake
    let cmakeArgs = [];
    if (platform === 'windows') {
      cmakeArgs = ['-G', 'Visual Studio 17 2022', '-A', 'x64'];
    } else if (platform === 'wasm32') {
      cmakeArgs = ['-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug'];
      // Emscripten toolchain is provided via emcmake wrapper
    }
    cmakeArgs.push('..');

    if (platform === 'wasm32') {
      execSync(`emcmake cmake ${cmakeArgs.join(' ')}`, {
        cwd: buildPath,
        stdio: 'inherit'
      });
    } else {
      execSync(`cmake ${cmakeArgs.join(' ')}`, {
        cwd: buildPath,
        stdio: 'inherit'
      });
    }

    console.log(chalk.green('✓ CMake configuration done'));

    // Step 2: Build
    console.log(chalk.blue('\n[2/3] Building project...'));
    if (platform === 'wasm32') {
      execSync(`cmake --build .`, {
        cwd: buildPath,
        stdio: 'inherit'
      });
    } else {
      const config = 'Debug';
      execSync(`cmake --build . --config ${config}`, {
        cwd: buildPath,
        stdio: 'inherit'
      });
    }
    console.log(chalk.green('✓ Build done'));

    // Step 3: Prepare runtime
    console.log(chalk.blue('\n[3/3] Preparing runtime...'));

    if (platform === 'wasm32') {
      const htmlFile = path.join(buildPath, 'proj', `${projectName}.html`);
      const wasmFile = path.join(buildPath, 'proj', `${projectName}.wasm`);

      if (await fs.pathExists(htmlFile)) {
        console.log(chalk.green(`✓ Output: ${htmlFile}`));
      } else if (await fs.pathExists(wasmFile)) {
        console.log(chalk.green(`✓ Output: ${wasmFile}`));
      } else {
        console.log(chalk.yellow(`Warning: Wasm output not found in ${path.join(buildPath, 'proj')}`));
      }

      if (skipRun) {
        console.log(chalk.green('\n✓ Project compiled successfully!'));
      } else {
        console.log(chalk.green('\n✓ Project ready to serve!'));
        console.log(chalk.blue('\nTo run the project, serve with a local HTTP server:'));
        console.log(`  cd ${path.join(buildPath, 'proj')}`);
        console.log(`  python -m http.server 8080`);
        console.log(`  # Then open http://localhost:8080/${projectName}.html`);
      }
    } else {
      // Native executable flow
      const exeName = projectName + (platform === 'windows' ? '.exe' : '');
      const config = 'Debug';
      let exePath = path.join(buildPath, config, exeName);

      if (!await fs.pathExists(exePath)) {
        exePath = path.join(buildPath, exeName);
      }

      if (await fs.pathExists(exePath)) {
        console.log(chalk.green(`✓ Executable: ${exePath}`));
      } else {
        console.log(chalk.yellow(`Warning: Executable not found at ${exePath}`));
        console.log(chalk.yellow('You may need to copy it manually to the data directory.'));
      }

      let runDir = projectPath;
      if (defaultDataDir) {
        if (await fs.pathExists(defaultDataDir)) {
          runDir = defaultDataDir;
          console.log(chalk.green(`✓ Running from directory: ${runDir}`));
        }
      }

      if (skipRun) {
        console.log(chalk.green('\n✓ Project compiled successfully!'));
      } else {
        console.log(chalk.green('\n✓ Project ready to run!'));
        console.log(chalk.blue('\nTo run the project:'));
        console.log(`  cd ${runDir}`);
        console.log(`  ${exePath}`);
      }
    }

  } catch (error) {
    console.error(chalk.red(`Error: ${error.message}`));
    process.exit(1);
  }
}

module.exports = runCommand;
