// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';

import 'package:unified_analytics/unified_analytics.dart';

import '../android/android_builder.dart';
import '../android/android_sdk.dart';
import '../android/gradle_utils.dart';
import '../base/common.dart';
import '../base/file_system.dart';
import '../base/os.dart';
import '../build_info.dart';
import '../cache.dart';
import '../globals.dart' as globals;
import '../project.dart';
import '../runner/flutter_command.dart' show FlutterCommandResult;
import 'build.dart';

class BuildAarCommand extends BuildSubCommand {
  BuildAarCommand({
    required super.logger,
    required AndroidSdk? androidSdk,
    required FileSystem fileSystem,
    required bool verboseHelp,
  }) : _androidSdk = androidSdk,
       _fileSystem = fileSystem,
       super(verboseHelp: verboseHelp) {
    argParser
      ..addFlag('debug', defaultsTo: true, help: 'Build a debug version of the current project.')
      ..addFlag(
        'profile',
        defaultsTo: true,
        help: 'Build a version of the current project specialized for performance profiling.',
      )
      ..addFlag(
        'release',
        defaultsTo: true,
        help: 'Build a release version of the current project.',
      )
      ..addOption(
        'publish',
        help: 'Publish the AAR to a maven repository specified by a JSON configuration file.',
      );
    addTreeShakeIconsFlag();
    usesFlavorOption();
    usesBuildNumberOption();
    usesOutputDir();
    usesPubOption();
    addSplitDebugInfoOption();
    addDartObfuscationOption();
    usesDartDefineOption();
    usesExtraDartFlagOptions(verboseHelp: verboseHelp);
    usesTrackWidgetCreation(verboseHelp: false);
    addEnableExperimentation(hide: !verboseHelp);
    addAndroidSpecificBuildOptions(hide: !verboseHelp);
    argParser.addMultiOption(
      'target-platform',
      defaultsTo: <String>['android-arm', 'android-arm64', 'android-x64'],
      allowed: <String>['android-arm', 'android-arm64', 'android-x64'],
      help: 'The target platform for which the project is compiled.',
    );
  }
  final AndroidSdk? _androidSdk;
  final FileSystem _fileSystem;

  @override
  final name = 'aar';

  @override
  Future<Set<DevelopmentArtifact>> get requiredArtifacts async => <DevelopmentArtifact>{
    DevelopmentArtifact.androidGenSnapshot,
  };

  @override
  late final FlutterProject project = _getProject();

  @override
  Future<Event> unifiedAnalyticsUsageValues(String commandPath) async {
    final String projectType;
    if (project.manifest.isModule) {
      projectType = 'module';
    } else if (project.manifest.isPlugin) {
      projectType = 'plugin';
    } else {
      projectType = 'app';
    }

    return Event.commandUsageValues(
      workflow: commandPath,
      commandHasTerminal: hasTerminal,
      buildAarProjectType: projectType,
      buildAarTargetPlatform: stringsArg('target-platform').join(','),
      // TODO(gmackall): Consider collecting hcpp analytics, see https://github.com/flutter/flutter/issues/184541.
    );
  }

  @override
  final description =
      'Build a repository containing an AAR and a POM file.\n\n'
      'By default, AARs are built for `release`, `debug` and `profile`.\n'
      'The POM file is used to include the dependencies that the AAR was compiled against.\n'
      'To learn more about how to use these artifacts, see: https://flutter.dev/to/integrate-android-archive\n'
      'This command assumes that the entrypoint is "lib/main.dart". '
      'This cannot currently be configured.';

  @override
  Future<void> validateCommand() async {
    if (!project.manifest.isModule) {
      throwToolExit('AARs can only be built from modules.');
    }
    await super.validateCommand();
  }

  @override
  bool get regeneratePlatformSpecificToolingDuringVerify => false;

  @override
  Future<FlutterCommandResult> runCommand() async {
    if (_androidSdk == null) {
      exitWithNoSdkMessage();
    }
    final androidBuildInfo = <AndroidBuildInfo>{};

    final Iterable<AndroidArch> targetArchitectures = stringsArg(
      'target-platform',
    ).map<AndroidArch>(getAndroidArchForName);

    final String? buildNumberArg = stringArg('build-number');
    final String buildNumber =
        argParser.options.containsKey('build-number') &&
            buildNumberArg != null &&
            buildNumberArg.isNotEmpty
        ? buildNumberArg
        : '1.0';

    final File targetFile = _fileSystem.file(_fileSystem.path.join('lib', 'main.dart'));
    for (final buildMode in const <String>['debug', 'profile', 'release']) {
      if (boolArg(buildMode)) {
        androidBuildInfo.add(
          AndroidBuildInfo(
            await getBuildInfo(
              forcedBuildMode: BuildMode.fromCliName(buildMode),
              forcedTargetFile: targetFile,
            ),
            targetArchs: targetArchitectures,
          ),
        );
      }
    }
    if (androidBuildInfo.isEmpty) {
      throwToolExit('Please specify a build mode and try again.');
    }

    String? repositoryUrl;
    String? repositoryUsername;
    String? repositoryPassword;

    if (argResults?.wasParsed('publish') ?? false) {
      final String publishFilePath = stringArg('publish')!;
      final File publishFile = _fileSystem.file(publishFilePath);
      if (!publishFile.existsSync()) {
        throwToolExit('The publish configuration file "$publishFilePath" does not exist.');
      }

      final String fileContent = publishFile.readAsStringSync();
      dynamic jsonConfig;
      try {
        jsonConfig = json.decode(fileContent);
      } on FormatException catch (e) {
        throwToolExit(
          'The publish configuration file "$publishFilePath" is not a valid JSON file: $e',
        );
      }

      if (jsonConfig is! Map<String, dynamic>) {
        throwToolExit('The publish configuration file "$publishFilePath" must be a JSON object.');
      }

      final dynamic repoUrlValue = jsonConfig['repoUrl'];
      if (repoUrlValue == null || repoUrlValue is! String || repoUrlValue.isEmpty) {
        throwToolExit(
          'The publish configuration file "$publishFilePath" must contain a non-empty string "repoUrl".',
        );
      }
      repositoryUrl = repoUrlValue;

      final dynamic usernameValue = jsonConfig['username'];
      if (usernameValue != null) {
        if (usernameValue is! String || usernameValue.isEmpty) {
          throwToolExit(
            'The "username" in the publish configuration file "$publishFilePath" must be a non-empty string.',
          );
        }
        repositoryUsername = usernameValue;
      }

      final dynamic passwordValue = jsonConfig['password'];
      if (passwordValue != null) {
        if (passwordValue is! String || passwordValue.isEmpty) {
          throwToolExit(
            'The "password" in the publish configuration file "$publishFilePath" must be a non-empty string.',
          );
        }
        repositoryPassword = passwordValue;
      }

      if ((repositoryUsername != null && repositoryPassword == null) ||
          (repositoryUsername == null && repositoryPassword != null)) {
        throwToolExit(
          'Both "username" and "password" must be provided in the publish configuration file "$publishFilePath" if either is specified.',
        );
      }
    }

    await androidBuilder?.buildAar(
      project: project,
      target: targetFile.path,
      androidBuildInfo: androidBuildInfo,
      generateTooling: regeneratePlatformSpecificToolingIfApplicable,
      outputDirectoryPath: stringArg('output'),
      buildNumber: buildNumber,
      repositoryUrl: repositoryUrl,
      repositoryUsername: repositoryUsername,
      repositoryPassword: repositoryPassword,
    );

    final bool impellerEnabled = project.android.computeImpellerEnabled();
    final buildLabel = impellerEnabled
        ? 'manifest-aar-impeller-enabled'
        : 'manifest-aar-impeller-disabled';
    globals.analytics.send(Event.flutterBuildInfo(label: buildLabel, buildType: 'android'));

    return FlutterCommandResult.success();
  }

  /// Returns the [FlutterProject] which is determined from the remaining command-line
  /// argument if any or the current working directory.
  FlutterProject _getProject() {
    final List<String> remainingArguments = argResults!.rest;
    if (remainingArguments.isEmpty) {
      return super.project;
    }
    final File mainFile = _fileSystem.file(remainingArguments.first);
    final String path;
    if (!mainFile.existsSync()) {
      final Directory pathProject = _fileSystem.directory(remainingArguments.first);
      if (!pathProject.existsSync()) {
        throwToolExit('${remainingArguments.first} does not exist');
      }
      path = pathProject.path;
    } else {
      path = mainFile.parent.path;
    }
    final String? projectRoot = findProjectRoot(_fileSystem, path);
    if (projectRoot == null) {
      throwToolExit('${mainFile.parent.path} is not a valid flutter project');
    }
    return FlutterProject.fromDirectory(_fileSystem.directory(projectRoot));
  }
}
