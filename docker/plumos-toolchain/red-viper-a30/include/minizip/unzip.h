#ifndef PLUMOS_RED_VIPER_A30_MINIZIP_STUB_H
#define PLUMOS_RED_VIPER_A30_MINIZIP_STUB_H

#define UNZ_OK 0
#define UNZ_END_OF_LIST_OF_FILE (-100)

typedef void *unzFile;

typedef struct unz_file_info_s {
  unsigned long version;
  unsigned long version_needed;
  unsigned long flag;
  unsigned long compression_method;
  unsigned long dosDate;
  unsigned long crc;
  unsigned long compressed_size;
  unsigned long uncompressed_size;
  unsigned long size_filename;
  unsigned long size_file_extra;
  unsigned long size_file_comment;
  unsigned long disk_num_start;
  unsigned long internal_fa;
  unsigned long external_fa;
} unz_file_info;

static inline unzFile unzOpen(const char *path) {
  (void)path;
  return (unzFile)0;
}

static inline int unzClose(unzFile file) {
  (void)file;
  return UNZ_OK;
}

static inline int unzGoToFirstFile(unzFile file) {
  (void)file;
  return UNZ_END_OF_LIST_OF_FILE;
}

static inline int unzGoToNextFile(unzFile file) {
  (void)file;
  return UNZ_END_OF_LIST_OF_FILE;
}

static inline int unzGetCurrentFileInfo(unzFile file, unz_file_info *info,
                                        char *filename,
                                        unsigned long filename_size,
                                        void *extra_field,
                                        unsigned long extra_field_size,
                                        char *comment,
                                        unsigned long comment_size) {
  (void)file;
  (void)info;
  (void)filename;
  (void)filename_size;
  (void)extra_field;
  (void)extra_field_size;
  (void)comment;
  (void)comment_size;
  return UNZ_END_OF_LIST_OF_FILE;
}

static inline int unzOpenCurrentFile(unzFile file) {
  (void)file;
  return UNZ_END_OF_LIST_OF_FILE;
}

static inline int unzReadCurrentFile(unzFile file, void *buf, unsigned len) {
  (void)file;
  (void)buf;
  (void)len;
  return UNZ_END_OF_LIST_OF_FILE;
}

static inline int unzCloseCurrentFile(unzFile file) {
  (void)file;
  return UNZ_OK;
}

#endif
