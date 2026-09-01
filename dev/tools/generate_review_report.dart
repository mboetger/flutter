import 'dart:convert';
import 'dart:developer';
import 'dart:io';
import 'dart:isolate';

class WorkerArgs {
  WorkerArgs(this.query, this.token);

  final String query;
  final String? token;
}

Future<int> _fetchCountWorker(WorkerArgs args) async {
  Timeline.startSync('Fetch GitHub API', arguments: <String, String>{'query': args.query});
  try {
    final Uri url = Uri.parse(
      'https://api.github.com/search/issues?q=${Uri.encodeQueryComponent(args.query)}',
    );
    final HttpClientRequest request = await HttpClient().getUrl(url);
    request.headers.add('User-Agent', 'Flutter-Review-Report-Script');

    if (args.token != null && args.token!.isNotEmpty) {
      request.headers.add('Authorization', 'Bearer ${args.token}');
    }

    Timeline.startSync('Network Request');
    final HttpClientResponse response = await request.close();
    Timeline.finishSync();

    Timeline.startSync('Read Response Body');
    final String bodyStr = await response.transform(utf8.decoder).join();
    Timeline.finishSync();

    if (response.statusCode != 200) {
      throw Exception(
        'Failed fetching API for query "${args.query}": ${response.statusCode} - $bodyStr',
      );
    }

    Timeline.startSync('JSON Decode');
    final Object? data = jsonDecode(bodyStr);
    Timeline.finishSync();

    if (data is Map<String, Object?> && data.containsKey('total_count')) {
      return data['total_count']! as int;
    } else {
      throw Exception('Invalid JSON response: total_count not found.');
    }
  } finally {
    Timeline.finishSync();
  }
}

Future<int> getPRCount(String query, String? token) {
  return Isolate.run(() => _fetchCountWorker(WorkerArgs(query, token)));
}

// ignore_for_file: avoid_print
// Script requires printing to console for user usage.

void main() async {
  // Try to use GITHUB_TOKEN if available, otherwise it will run unauthenticated
  final String? token = Platform.environment['GITHUB_TOKEN'] ?? Platform.environment['GH_TOKEN'];

  if (token == null || token.isEmpty) {
    print('WARNING: No GITHUB_TOKEN or GH_TOKEN found in environment.');
    print('GitHub API heavily rate limits unauthenticated requests (10 req/min for search).');
  }

  const frameworkQuery =
      'repo:flutter/flutter is:open draft:false is:pr label:platform-android,team-android -label:"work in progress; do not review"';
  const pluginsQuery =
      'repo:flutter/packages is:open draft:false is:pr label:triage-android -author:app/dependabot';
  const dependabotQuery =
      'repo:flutter/packages is:open draft:false is:pr label:platform-android author:app/dependabot';

  const reviewers = <String>['jesswrd', 'mboetger', 'camsim99', 'gmackall'];

  print('Fetching PR counts... (this may take a moment)\n');

  try {
    // Execute all queries in parallel, utilizing multiple Isolates as per the concurrency guidelines.
    final List<int> results = await Future.wait(<Future<int>>[
      getPRCount(frameworkQuery, token),
      getPRCount(pluginsQuery, token),
      getPRCount(dependabotQuery, token),
      for (final String reviewer in reviewers)
        getPRCount('org:flutter is:open is:pr review-requested:$reviewer', token),
    ]);

    final int frameworkCount = results[0];
    final int pluginsCount = results[1];
    final int dependabotCount = results[2];

    print('=== Team Android PR Report ===');
    print('Framework PRs: $frameworkCount');
    print('Plugins PRs: $pluginsCount');
    print('Dependabot PRs: $dependabotCount');

    print('\n=== PRs Assigned for Review ===');
    for (var i = 0; i < reviewers.length; i++) {
      print('${reviewers[i]}: ${results[3 + i]} PR(s)');
    }
  } catch (e) {
    print('\nAn error occurred while generating the report:\n$e');
    exit(1);
  }
}
