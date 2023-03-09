struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint atime;  // last used time
  uint dev;
  uint blockno;  // 表示block号，
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];   // #define BSIZE 1024  // block size
};

