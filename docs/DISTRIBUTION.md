# Distribution requirements

The application's own code uses MIT. nlohmann/json uses MIT; GoogleTest uses a
BSD-style license and is test-only. Preserve actual upstream notices when bundling
dependencies, including compiler runtime libraries.

Qt licensing depends on the distribution used. A dynamically linked LGPL Qt
distribution requires the applicable notices, license texts, user relinking/
replacement rights and corresponding-source availability obligations. Qt Quick
imports and plugins must be included with their correct licenses. Alternatively,
use an appropriate commercial Qt license. Complete this review before release;
this document is not a substitute for actual license files and source obligations.

The manual workflow creates development artifacts only. Verify the extracted
archive and bundled runtime on a clean machine before labelling it installable.
Signing, installer generation and a Linux self-contained runtime are deferred.
