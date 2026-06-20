dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        val gradleRepos: String? = System.getenv("FLUTTER_GRADLE_REPOS")
        if (!gradleRepos.isNullOrBlank()) {
            gradleRepos.split(',', ';').forEach { repoUrl ->
                val trimmed = repoUrl.trim()
                if (trimmed.isNotEmpty()) {
                    maven { url = uri(trimmed) }
                }
            }
        } else {
            google()
            mavenCentral()
        }
    }
}
