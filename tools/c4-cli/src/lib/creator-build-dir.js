const path = require('path');

/**
 * Cocos Creator 导出目录名（build 下的子目录），与编辑器平台名一致。
 */
const CREATOR_BUILD_SUBDIR = {
  windows: 'windows',
  mac: 'mac',
  ios: 'ios',
  android: 'android',
  linux: 'linux'
};

/**
 * wasm32 无官方 Creator 导出目录：在 Windows 上用 windows 构建产物，在 macOS 上用 mac 构建产物。
 * @param {string} cliPlatform - c4-cli -p 值
 * @returns {string} build 下的子目录名
 */
function creatorBuildSubdirForCliPlatform(cliPlatform) {
  if (cliPlatform === 'wasm32') {
    if (process.platform === 'win32') {
      return CREATOR_BUILD_SUBDIR.windows;
    }
    if (process.platform === 'darwin') {
      return CREATOR_BUILD_SUBDIR.mac;
    }
    return CREATOR_BUILD_SUBDIR.linux;
  }
  return CREATOR_BUILD_SUBDIR[cliPlatform] || cliPlatform;
}

/**
 * Creator 工程根目录下的「构建输出」路径（运行时可执行文件的工作目录 / 资源目录通常在此）。
 * @param {string} creatorProjectRoot - Creator 工程根（含 assets、settings 等）
 * @param {string} cliPlatform - new/run 的 -p
 */
function resolveCreatorBuildOutputDir(creatorProjectRoot, cliPlatform) {
  const sub = creatorBuildSubdirForCliPlatform(cliPlatform);
  return path.join(path.resolve(creatorProjectRoot), 'build', sub);
}

module.exports = {
  creatorBuildSubdirForCliPlatform,
  resolveCreatorBuildOutputDir,
  CREATOR_BUILD_SUBDIR
};
