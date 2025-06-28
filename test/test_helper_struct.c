struct Stuff4 {
  int x, y, z, w;
};

struct LittleStuff {
  int x;
  float y;
};

static struct Stuff4 testhelper_returns_stuff4(void) {
  return (struct Stuff4){99, 97, 87, 66};
}

static void testhelper_takes_stuff4(struct Stuff4 s) {
  printf("s.x: %d\n", s.x);
  printf("s.y: %d\n", s.y);
  printf("s.z: %d\n", s.z);
  printf("s.w: %d\n", s.w);
}

static struct Stuff4 testhelper_takes_and_returns_stuff4(struct Stuff4 a, struct Stuff4 b) {
  printf("a.x: %d\n", a.x);
  printf("a.y: %d\n", a.y);
  printf("a.z: %d\n", a.z);
  printf("a.w: %d\n", a.w);
  printf("b.x: %d\n", b.x);
  printf("b.y: %d\n", b.y);
  printf("b.z: %d\n", b.z);
  printf("b.w: %d\n", b.w);
  return (struct Stuff4){2, 3, 5, 8};
}

static struct LittleStuff testhelper_returns_littlestuff(void) {
  return (struct LittleStuff){123, 3.14f};
}

static void testhelper_takes_littlestuff(struct LittleStuff ls) {
  printf("ls.x: %d\n", ls.x);
  printf("ls.y: %f\n", ls.y);
}

static struct LittleStuff testhelper_takes_and_returns_little_and_big(struct LittleStuff a, struct Stuff4 b, struct LittleStuff c) {
  printf("a.x: %d\n", a.x);
  printf("a.y: %f\n", a.y);
  printf("b.x: %d\n", b.x);
  printf("b.y: %d\n", b.y);
  printf("b.z: %d\n", b.z);
  printf("b.w: %d\n", b.w);
  printf("c.x: %d\n", c.x);
  printf("c.y: %f\n", c.y);
  return (struct LittleStuff){999, 888.0f};
}

