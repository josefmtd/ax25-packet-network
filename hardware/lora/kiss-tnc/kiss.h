# include <stdint.h>

class kiss {
private:
  uint8_t *const bufferBig, *const bufferSmall;
  const uint16_t maxPacketSize;
  bool (* peekRadio)();
  void (* getRadio)(uint8_t *const whereTo, uint16_t *const n);
  void (* putRadio)(const uint8_t *const what, const uint16_t size);
  uint16_t (* peekSerial)();
  bool (* getSerial)(uint8_t *const whereTo, const uint16_t n, const unsigned long int to);
  void (* putSerial)(const uint8_t *const what, const uint16_t size);
  bool (* resetRadio)();

  void processRadio();
  void processSerial();

public:
  kiss(const uint16_t maxPacketSize, bool (* peekRadio)(),
  void (* getRadio)(uint8_t *const whereTo, uint16_t *const n),
  void (* putRadio)(const uint8_t *const what, const uint16_t size),
  uint16_t (* peekSerial)(), bool (* getSerial)(uint8_t *const whereTo,
  const uint16_t n, const unsigned long int to),
  void (* putSerialIn)(const uint8_t *const what, const uint16_t size),
  bool (* resetRadioIn)());
  ~kiss();

  void debug(const char *const t);

  void begin();
  void loop();
};
