# CI/CD Pipeline Documentation

## GitHub Actions Workflows

Basketball Manager uses GitHub Actions for continuous integration and deployment.

### Workflows

#### 1. **CI Pipeline** (`.github/workflows/ci.yml`)

Runs on every push and pull request to `main` and `dev` branches.

**Jobs:**

- 🏗️ **cpp-build**: Builds C++ core libraries (bm-core)
  - Installs: CMake, Lua, SQLite3
  - Target: macOS ARM64 (M1 native)
  - Uploads build artifacts

- 🔨 **csharp-build**: Builds C# NCAA Data Importer
  - Setup: .NET 10.x
  - Tests import with sample data
  - Uploads compiled binaries

- 🔍 **code-quality**: Code quality checks
  - File permissions validation
  - Large file detection
  - Lua syntax validation
  - CSV structure verification

- 📚 **docs-check**: Documentation validation
  - README.md existence
  - Markdown link checking
  - Documentation completeness

- 🧪 **integration-test**: Integration testing
  - Downloads all artifacts
  - Tests SQLite operations
  - Verifies library linking

- ✅ **ci-status**: Summary job
  - Reports overall pipeline status
  - Fails if any job fails

**Status Badge:**
```markdown
![CI](https://github.com/RomanDivkovic/basketball-manager/workflows/Basketball%20Manager%20CI/badge.svg)
```

#### 2. **Release Build** (`.github/workflows/release.yml`)

Runs on new releases or manual trigger.

**Features:**
- Builds optimized Release binaries
- Packages C++ libraries + C# tools
- Includes documentation and data files
- Creates `.tar.gz` archive
- Uploads to GitHub Releases

**Manual Trigger:**
```bash
# Via GitHub UI: Actions → Release Build → Run workflow
# Specify version (e.g., v0.1.0)
```

### Pipeline Visualization

```
┌─────────────────────────────────────────────────────────┐
│                    Push/PR to main/dev                   │
└───────────────────────┬─────────────────────────────────┘
                        │
        ┌───────────────┼───────────────┬─────────────┐
        │               │               │             │
        ▼               ▼               ▼             ▼
  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
  │  C++     │   │  C#      │   │  Code    │   │  Docs    │
  │  Build   │   │  Build   │   │  Quality │   │  Check   │
  └─────┬────┘   └─────┬────┘   └─────┬────┘   └─────┬────┘
        │              │              │              │
        └──────────────┴──────────────┴──────────────┘
                         │
                         ▼
                  ┌────────────┐
                  │Integration │
                  │   Test     │
                  └─────┬──────┘
                        │
                        ▼
                  ┌────────────┐
                  │CI  Status  │
                  │  Summary   │
                  └────────────┘
```

## Local Testing

Before pushing, test locally:

### C++ Build
```bash
cd bm-core
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

### C# Build
```bash
cd bm-tools/NCAAImporter
dotnet build --configuration Release
dotnet test
```

### Lua Syntax Check
```bash
find . -name "*.lua" -exec lua -e "assert(loadfile('{}'))" \;
```

## Environment Variables

No secrets required currently. Future additions:

- `UNITY_LICENSE`: For Unity builds (Phase 3+)
- `DEPLOY_TOKEN`: For automated deployment
- `CODECOV_TOKEN`: For code coverage reporting

## Artifacts

Build artifacts are stored for 7 days:

- **cpp-libs**: C++ compiled libraries (.dylib, .a)
- **csharp-importer**: C# executable and dependencies
- **basketball-manager-macos-arm64**: Complete release package

Download from Actions tab → Workflow run → Artifacts section.

## Triggers

### Automatic
- Push to `main` or `dev` branch
- Pull request to `main` or `dev` branch
- New release created

### Manual
- Navigate to: Actions → Workflow → "Run workflow"
- Available for both CI and Release workflows

## Build Matrix (Future)

Currently building for:
- ✅ macOS ARM64 (M1/M2/M3)

Future platforms:
- 🔜 macOS x64 (Intel)
- 🔜 Linux x64 (Ubuntu)
- 🔜 Windows x64

## Performance

Typical build times:
- C++ Build: ~2 minutes
- C# Build: ~1 minute
- Code Quality: ~30 seconds
- Integration Test: ~1 minute
- **Total**: ~5-6 minutes

Free tier limits:
- 2000 minutes/month (private repos)
- Unlimited (public repos)

## Troubleshooting

### CMake not found
```bash
# Install via Homebrew
brew install cmake
```

### .NET version mismatch
```yaml
# Update in workflow:
dotnet-version: '10.x'  # or '8.x'
```

### Lua syntax errors
```bash
# Check specific file:
lua -e "assert(loadfile('path/to/file.lua'))"
```

### Artifact download fails
- Check retention period (7 days default)
- Verify job completed successfully
- Check GitHub storage limits

## Badges

Add to README.md:

```markdown
![CI Status](https://github.com/RomanDivkovic/basketball-manager/workflows/Basketball%20Manager%20CI/badge.svg)
![Release](https://github.com/RomanDivkovic/basketball-manager/workflows/Release%20Build/badge.svg)
![License](https://img.shields.io/github/license/RomanDivkovic/basketball-manager)
![Stars](https://img.shields.io/github/stars/RomanDivkovic/basketball-manager)
```

## Next Steps

Planned improvements:
- [ ] Add unit tests (C++ GoogleTest, C# xUnit)
- [ ] Code coverage reporting (Codecov)
- [ ] Unity build pipeline (Phase 3)
- [ ] Cross-platform builds (Linux, Windows)
- [ ] Automated deployment
- [ ] Performance benchmarks
- [ ] Docker containers for consistent builds

## Resources

- [GitHub Actions Docs](https://docs.github.com/en/actions)
- [.NET Actions](https://github.com/actions/setup-dotnet)
- [CMake Actions](https://github.com/marketplace/actions/run-cmake)
- [Artifact Actions](https://github.com/actions/upload-artifact)
