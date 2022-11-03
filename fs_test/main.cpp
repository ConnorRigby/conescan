#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <emscripten.h>

#define MOUNTPOINT "/work/"

int main() {
    FILE *fp;
    char buffer[100];
    struct stat stats;
    char c[] = "this is a test\n";

    EM_ASM(
        // Make a directory other than '/'
        FS.mkdir("/work");
        // Then mount with IDBFS type
        FS.mount(IDBFS, {}, "/work");

        // Then sync
        FS.syncfs(true, function (err) {
            assert(!err);
        });
    );
    printf("synced fs\n");
    emscripten_sleep(5000);
    if (stat(MOUNTPOINT"file.txt", &stats) == 0) {
        printf("file exists!\n");

        printf("reading file\n");
        fp = fopen(MOUNTPOINT"file.txt", "r");
        assert(fp);
        fread(buffer, strlen(c)+1, 1, fp);
        printf("read file: %s\n", buffer);
        fclose(fp);
    } else {
        printf("file does not exist\n");

        printf("opening file for write\n");
        fp = fopen(MOUNTPOINT"file.txt", "w");
        assert(fp);

        fwrite(c, strlen(c) + 1, 1, fp);
        printf("wrote file");
        fclose(fp);

        printf("reading file\n");
        fp = fopen(MOUNTPOINT"file.txt", "r");
        assert(fp);
        fread(buffer, strlen(c)+1, 1, fp);
        printf("%s\n", buffer);
        fclose(fp);
        sync();
    }
    EM_ASM(
        FS.syncfs(function (err) {
            assert(!err);
        });
    );
    return 0;
}