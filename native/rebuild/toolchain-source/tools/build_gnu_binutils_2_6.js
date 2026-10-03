#!/usr/bin/env node
'use strict';

const childProcess = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const CONFIG_FILE = path.join(ROOT, 'config', 'gnu-binutils-2.6-build.json');

function fail(message) {
  throw new Error(`GNU Binutils 2.6 build failure: ${message}`);
}

function readJson(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function sha256File(file) {
  return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex').toUpperCase();
}

function normalize(value) {
  return value.replace(/\\/g, '/');
}

function value(flag) {
  const index = process.argv.indexOf(flag);
  if (index < 0 || !process.argv[index + 1]) fail(`missing ${flag}`);
  return path.resolve(process.argv[index + 1]);
}

function parseArgs() {
  if (process.argv.includes('--help') || process.argv.includes('-h')) {
    console.log('Usage: node tools/build_gnu_binutils_2_6.js --source <pinned-checkout> --msys-root <msys64> --work <empty-external-dir> --output <empty-bundle-dir>');
    process.exit(0);
  }
  return {
    source: value('--source'),
    msysRoot: value('--msys-root'),
    work: value('--work'),
    output: value('--output'),
  };
}

function ensureEmpty(directory, label) {
  if (fs.existsSync(directory) && fs.readdirSync(directory).length !== 0) fail(`${label} must be absent or empty`);
  fs.mkdirSync(directory, { recursive: true });
}

function run(executable, args, options = {}) {
  const result = childProcess.spawnSync(executable, args, {
    cwd: options.cwd || ROOT,
    env: options.env || process.env,
    encoding: 'utf8',
    windowsHide: true,
    maxBuffer: 128 * 1024 * 1024,
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    fail(`command exited ${result.status}: ${executable} ${args.join(' ')}\n${result.stdout || ''}${result.stderr || ''}`);
  }
  return { command: [executable, ...args], stdout: result.stdout, stderr: result.stderr };
}

function verifyFile(file, expected, label) {
  if (!fs.existsSync(file)) fail(`${label} is missing: ${file}`);
  const actual = { bytes: fs.statSync(file).size, sha256: sha256File(file) };
  if (expected && (actual.bytes !== expected.bytes || actual.sha256 !== expected.sha256)) {
    fail(`${label} identity drift: ${actual.bytes} bytes ${actual.sha256}`);
  }
  return actual;
}

function verifyBuildHostPackages(msysRoot, packages) {
  if (!Array.isArray(packages) || packages.length === 0) fail('MSYS2 package inventory is missing');
  const databaseRoot = path.join(msysRoot, 'var', 'lib', 'pacman', 'local');
  const cacheRoot = path.join(msysRoot, 'var', 'cache', 'pacman', 'pkg');
  const cacheFiles = fs.readdirSync(cacheRoot);
  return packages.map((record) => {
    if (!record || typeof record.name !== 'string' || typeof record.version !== 'string'
        || !/^[0-9A-F]{64}$/.test(record.archiveSha256)) fail('MSYS2 package record is malformed');
    const installed = path.join(databaseRoot, `${record.name}-${record.version}`);
    if (!fs.existsSync(installed) || !fs.statSync(installed).isDirectory()) {
      fail(`MSYS2 package is not installed at the pinned version: ${record.name} ${record.version}`);
    }
    const prefix = `${record.name}-${record.version}-`;
    const matches = cacheFiles.filter((name) => name.startsWith(prefix) && /\.pkg\.tar\.(?:zst|xz)$/.test(name));
    if (matches.length !== 1) fail(`MSYS2 package archive does not resolve uniquely: ${record.name} ${record.version}`);
    const archive = path.join(cacheRoot, matches[0]);
    const actual = { bytes: fs.statSync(archive).size, sha256: sha256File(archive) };
    if (actual.sha256 !== record.archiveSha256) fail(`MSYS2 package archive identity drift: ${matches[0]}`);
    return { name: record.name, version: record.version, archive: matches[0], ...actual };
  });
}

function quoteBash(value) {
  return `'${String(value).replace(/'/g, `'"'"'`)}'`;
}

function main() {
  const args = parseArgs();
  const config = readJson(CONFIG_FILE);
  if (config.schemaVersion !== 1 || !config.source || !config.configure || !config.outputs) fail('build configuration schema drift');
  if (!fs.existsSync(path.join(args.source, '.git'))) fail('source is not a Git checkout');
  const sourceHead = run('git', ['rev-parse', 'HEAD'], { cwd: args.source }).stdout.trim();
  if (sourceHead !== config.source.commit) fail(`source commit drift: ${sourceHead}`);
  const sourceStatus = run('git', ['status', '--porcelain'], { cwd: args.source }).stdout.trim();
  if (sourceStatus) fail('input source checkout is dirty');

  const bash = path.join(args.msysRoot, 'usr', 'bin', 'bash.exe');
  const runner = path.join(args.msysRoot, 'usr', 'bin', 'msys-2.0.dll');
  verifyFile(runner, config.outputs['bin/msys-2.0.dll'], 'MSYS2 runner');
  const packages = verifyBuildHostPackages(args.msysRoot, config.buildHost && config.buildHost.packages);
  if (!fs.existsSync(bash)) fail(`MSYS2 bash is missing: ${bash}`);
  if (!config.buildScript || config.buildScript.path !== 'tools/build_gnu_binutils_2_6.js'
      || sha256File(__filename) !== config.buildScript.sha256) fail('build-script identity drift');
  for (const patchConfig of [config.buildPatch, config.structuralLinkerPatch, config.binaryLmaPatch, config.hi16PairingPatch]) {
    const patchFile = path.join(ROOT, ...patchConfig.path.split('/'));
    if (sha256File(patchFile) !== patchConfig.sha256) fail(`patch identity drift: ${patchConfig.path}`);
  }

  ensureEmpty(args.work, 'work directory');
  ensureEmpty(args.output, 'output directory');
  const buildSource = path.join(args.work, 'source');
  const commands = [];
  commands.push(run('git', ['clone', '--no-hardlinks', '--no-checkout', args.source, buildSource], { cwd: args.work }));
  commands.push(run('git', ['checkout', '--detach', config.source.commit], { cwd: buildSource }));
  for (const patchConfig of [config.buildPatch, config.structuralLinkerPatch, config.binaryLmaPatch, config.hi16PairingPatch]) {
    commands.push(run('git', ['apply', path.join(ROOT, ...patchConfig.path.split('/'))], { cwd: buildSource }));
  }

  const stableEnv = {
    ...process.env,
    CHERE_INVOKING: '1',
    LC_ALL: 'C',
    MSYSTEM: 'MSYS',
    SOURCE_DATE_EPOCH: '0',
    TZ: 'UTC',
  };
  const configureFlags = [
    `--target=${config.configure.target}`,
    `--host=${config.configure.host}`,
    `--build=${config.configure.build}`,
    ...config.configure.flags,
  ];
  const buildCommands = [
    `CFLAGS=${quoteBash(config.configure.cflags)} ./configure ${configureFlags.map(quoteBash).join(' ')}`,
    'make -C ld -W ldgram.y ldgram.c',
    `make -j1 CFLAGS=${quoteBash(config.configure.cflags)} all-binutils all-gas all-ld`,
    `make -C bfd -W elf.c CFLAGS=${quoteBash(config.configure.elfBfdCflags)} elf.o`,
    `make -C bfd -W binary.c CFLAGS=${quoteBash(config.configure.binaryBfdCflags)} binary.o`,
    `make -C bfd -W elf32-mips.c CFLAGS=${quoteBash(config.configure.mipsElfBfdCflags)} elf32-mips.o`,
    `make -C bfd CFLAGS=${quoteBash(config.configure.cflags)} libbfd.a`,
    `make -C ld CFLAGS=${quoteBash(config.configure.cflags)} ld.new`,
    `make -C binutils CFLAGS=${quoteBash(config.configure.cflags)} objcopy objdump nm.new size strings strip.new`,
  ];
  for (const command of buildCommands) commands.push(run(bash, ['-lc', command], { cwd: buildSource, env: stableEnv }));

  const bin = path.join(args.output, 'bin');
  fs.mkdirSync(bin, { recursive: true });
  const products = {
    'bin/mips-kmc-elf-as.exe': path.join(buildSource, 'gas', 'as.new'),
    'bin/mips-kmc-elf-ld.exe': path.join(buildSource, 'ld', 'ld.new'),
    'bin/mips-kmc-elf-nm.exe': path.join(buildSource, 'binutils', 'nm.new'),
    'bin/mips-kmc-elf-objcopy.exe': path.join(buildSource, 'binutils', 'objcopy.exe'),
    'bin/mips-kmc-elf-objdump.exe': path.join(buildSource, 'binutils', 'objdump.exe'),
    'bin/mips-kmc-elf-size.exe': path.join(buildSource, 'binutils', 'size.exe'),
    'bin/mips-kmc-elf-strings.exe': path.join(buildSource, 'binutils', 'strings.exe'),
    'bin/mips-kmc-elf-strip.exe': path.join(buildSource, 'binutils', 'strip.new'),
    'bin/msys-2.0.dll': runner,
  };
  const outputs = {};
  for (const [relative, source] of Object.entries(products)) {
    const destination = path.join(args.output, ...relative.split('/'));
    if (!fs.existsSync(source)) fail(`built product is missing: ${source}`);
    fs.copyFileSync(source, destination);
    outputs[relative] = verifyFile(destination, config.outputs[relative], `built product ${relative}`);
  }

  const versions = {};
  for (const [name, relative] of Object.entries({
    assembler: 'bin/mips-kmc-elf-as.exe',
    linker: 'bin/mips-kmc-elf-ld.exe',
    objcopy: 'bin/mips-kmc-elf-objcopy.exe',
    objdump: 'bin/mips-kmc-elf-objdump.exe',
  })) {
    const executable = path.join(args.output, ...relative.split('/'));
    const flag = name === 'assembler' ? '--version' : '--version';
    const versionResult = run(executable, [flag], { cwd: args.output, env: stableEnv });
    versions[name] = `${versionResult.stdout}${versionResult.stderr}`.split(/\r?\n/)[0].trim()
      .replace(/^GNU .*mips-kmc-elf-(objcopy|objdump)/, 'GNU $1');
    if (versions[name] !== config.versions[name]) fail(`${name} version drift: ${versions[name]}`);
  }

  const report = {
    schemaVersion: 1,
    status: 'pass',
    source: { path: normalize(args.source), commit: sourceHead },
    work: normalize(args.work),
    output: normalize(args.output),
    buildHost: {
      bash: normalize(bash),
      gcc: run(bash, ['-lc', 'gcc --version | sed -n 1p'], { cwd: buildSource, env: stableEnv }).stdout.trim(),
      make: run(bash, ['-lc', 'make --version | sed -n 1p'], { cwd: buildSource, env: stableEnv }).stdout.trim(),
      runner: outputs['bin/msys-2.0.dll'],
      packages,
    },
    patches: [config.buildPatch, config.structuralLinkerPatch, config.binaryLmaPatch, config.hi16PairingPatch],
    commands: commands.map((record) => record.command.map(normalize)),
    versions,
    outputs,
  };
  fs.writeFileSync(path.join(args.output, 'build-report.json'), `${JSON.stringify(report, null, 2)}\n`);
  console.log(`GNU Binutils 2.6 build: PASS (${config.source.commit})`);
  console.log(`Bundle: ${args.output}`);
}

try {
  main();
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
