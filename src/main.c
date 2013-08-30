/*
* Copyright (c) 2012-2013 Spotify AB
*
* Licensed under the Apache License, Version 2.0 (the "License"); you may not
* use this file except in compliance with the License. You may obtain a copy of
* the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations under
* the License.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "logheader.h"
#include "hashheader.h"
#include "sparkey.h"

void usage() {
	printf("Usage: sparkey <command> <options>\n");
	printf("Commands: info [file...]\n");
	printf("Commands: get <index file> <key>\n");
}

int info(int argv, const char **args) {
  int retval = 0;
  sparkey_logheader logheader;
  sparkey_hashheader hashheader;
  for (int i = 0; i < argv; i++) {
    const char *filename = args[i];
    sparkey_returncode res = sparkey_load_logheader(&logheader, filename);
    if (res == SPARKEY_SUCCESS) {
      printf("%s\n", filename);
      print_logheader(&logheader);
    } else {
      sparkey_returncode res2 = sparkey_load_hashheader(&hashheader, filename);
      if (res2 == SPARKEY_SUCCESS) {
        printf("%s\n", args[i]);
        print_hashheader(&hashheader);
      } else {
        printf("%s is neither a sparkey log file (%s) nor an index file (%s)\n", filename, sparkey_errstring(res), sparkey_errstring(res2));
        retval = 1;
      }
    }
  }
  return retval;
}

int get(const char *hashfile, const char *logfile, const char *key) {
  sparkey_hashreader *reader;
  sparkey_logreader *logreader;
  sparkey_logiter *iter;
  sparkey_returncode errcode = sparkey_hash_open(&reader, hashfile, logfile);
  if (errcode != SPARKEY_SUCCESS) {
    puts(sparkey_errstring(errcode));
    puts("\n");
    return 1;
  }
  logreader = sparkey_hash_getreader(reader);
  errcode = sparkey_logiter_create(&iter, logreader);
  if (errcode != SPARKEY_SUCCESS) {
    sparkey_hash_close(&reader);
    puts(sparkey_errstring(errcode));
    puts("\n");
    return 1;
  }

  uint64_t keylen = strlen(key);
  errcode = sparkey_hash_get(reader, (uint8_t*) key, keylen, iter);
  if (errcode != SPARKEY_SUCCESS) {
    sparkey_logiter_close(&iter);
    sparkey_hash_close(&reader);
    puts(sparkey_errstring(errcode));
    puts("\n");
    return 1;
  }

  int exitcode = 2;
  if (sparkey_logiter_state(iter) == SPARKEY_ITER_ACTIVE) {
    exitcode = 0;
    uint8_t * res;
    uint64_t len;
    do {
      errcode = sparkey_logiter_valuechunk(iter, logreader, 1 << 31, &res, &len);
      if (errcode != SPARKEY_SUCCESS) {
        sparkey_logiter_close(&iter);
        sparkey_hash_close(&reader);
        puts(sparkey_errstring(errcode));
        puts("\n");
        return 1;
      }
      fwrite(res, 1, len, stdout);
    } while (len > 0);
  }
  sparkey_logiter_close(&iter);
  sparkey_hash_close(&reader);
  return exitcode;
}

int main(int argv, const char **args) {
  if (argv < 2) {
    usage();
    return 1;
  }
  if (strcmp(args[1], "info") == 0) {
    if (argv < 3) {
      usage();
      return 1;
    }
    return info(argv - 2, args + 2);
  } else if (strcmp(args[1], "get") == 0) {
    if (argv < 4) {
      usage();
      return 1;
    }
    const char *index_filename = args[2];
    char *log_filename = sparkey_create_log_filename(index_filename);
    if (log_filename == NULL) {
      printf("index filename must end with .spi\n");
      return 1;
    }
    int retval = get(args[2], log_filename, args[3]);
    free(log_filename);
    return retval;
  } else {
    printf("Unknown command: %s\n", args[1]);
    usage();
    return 1;
  }
}
