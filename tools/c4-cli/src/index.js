#!/usr/bin/env node

const { program } = require('commander');
const path = require('path');

program
  .name('cocos')
  .description('Cocos Creator Native CLI')
  .version('1.0.0');

program
  .command('run')
  .description('Build and run the project')
  .option('-p, --platform <platform>', 'Target platform (windows, android, ios, linux, mac, wasm32)', 'windows')
  .option('-b, --build-dir <dir>', 'Build directory', 'build')
  .option('--data-dir <dir>', 'Data directory from Cocos Creator build')
  .action(async (options) => {
    const runCommand = require('./commands/run');
    await runCommand(process.cwd(), options);
  });

program
  .command('compile')
  .description('Compile the project (alias for run without executing)')
  .option('-p, --platform <platform>', 'Target platform (windows, android, ios, linux, mac, wasm32)', 'windows')
  .option('-b, --build-dir <dir>', 'Build directory', 'build')
  .action(async (options) => {
    const runCommand = require('./commands/run');
    options.skipRun = true;
    await runCommand(process.cwd(), options);
  });

program
  .command('new <projectName>')
  .description('Create a new Cocos Native project')
  .option('-p, --platform <platform>', 'Target platform (windows, android, ios, linux, mac, wasm32)', 'windows')
  .option('-P, --package <packageName>', 'Package name for Android (default: com.cocos.helloworld)', 'com.cocos.helloworld')
  .option('-d, --directory <directory>', 'Project output directory', '.')
  .option('-e, --engine <path>', 'Engine directory path (default: auto-detect from project location)')
  .option('--data-dir <dir>', 'Default data directory for running the project')
  .action(async (projectName, options) => {
    const newCommand = require('./commands/new');
    await newCommand(projectName, options);
  });

program.parse(process.argv);
