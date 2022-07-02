#include <map>

class CACerts
{
  public:
    static const char * getCert(const String& host);

  private:
    static const std::map<String, const char *> ca_certs;
};

extern std::map<String, const char *> ca_certs;
