#include "CurrentConditions.h"

#include <ArduinoLog.h>
#include <WiFiClientSecure.h>

#include <cstdio>

#include "CACerts.h"

static const char* url_format = "https://api.weather.gov/stations/%s/observations/latest";
static unsigned int json_size = 768;

CurrentConditions::CurrentConditions(std::shared_ptr<Network> network, const String station)
    : m_network(network), m_station(station), m_doc(json_size) {
  char buffer[256];
  sprintf(buffer, url_format, m_station.c_str());
  if (!m_url.setUrl(buffer)) {
    Log.error(F("Failuer to set URL: %s" CR), buffer);
  }

  JsonObject filter_properties = m_filter.createNestedObject("properties");
  filter_properties["textDescription"] = true;
  filter_properties["rawMessage"] = true;
  filter_properties["windSpeed"] = true;
  filter_properties["dewpoint"] = true;
  filter_properties["temperature"] = true;

  Log.notice(F("Set Current Conditions URL to %s" CR), m_url.getUrl().c_str());
}

int CurrentConditions::update() {
  WiFiClientSecure client;
  StreamString stream;
  client.setCACert(CACerts::getCert(m_url.getHost()));

  Log.notice(F("Getting Current Conditions" CR));
  m_network->get(client, m_url.getUrl(), stream);
  return this->parse(stream);
}

int CurrentConditions::parse(Stream& input) {
  // Stream& input;

  // Parse code generated with https://arduinojson.org/v6/assistant/
  DeserializationError error = deserializeJson(m_doc, input, DeserializationOption::Filter(m_filter));

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return CURRENT_CONDITIONS_ERROR;
  }

  JsonObject properties = m_doc["properties"];
  const char* properties_rawMessage = properties["rawMessage"];            // "KBFI 222353Z 17008KT 10SM SCT020 ...
  const char* properties_textDescription = properties["textDescription"];  // "Cloudy"

  JsonObject properties_temperature = properties["temperature"];
  const char* properties_temperature_unitCode = properties_temperature["unitCode"];              // "wmoUnit:degC"
  float properties_temperature_value = properties_temperature["value"];                          // 12.2
  const char* properties_temperature_qualityControl = properties_temperature["qualityControl"];  // "V"

  JsonObject properties_dewpoint = properties["dewpoint"];
  const char* properties_dewpoint_unitCode = properties_dewpoint["unitCode"];              // "wmoUnit:degC"
  float properties_dewpoint_value = properties_dewpoint["value"];                          // 10.6
  const char* properties_dewpoint_qualityControl = properties_dewpoint["qualityControl"];  // "V"

  JsonObject properties_windSpeed = properties["windSpeed"];
  const char* properties_windSpeed_unitCode = properties_windSpeed["unitCode"];              // "wmoUnit:km_h-1"
  float properties_windSpeed_value = properties_windSpeed["value"];                          // 14.76
  const char* properties_windSpeed_qualityControl = properties_windSpeed["qualityControl"];  // "V"

  // End Json auto-generated code

  description = properties_textDescription;
  raw_message = properties_rawMessage;
  if (!(properties_temperature["value"] == nullptr)){
    temperature = properties_temperature_value;
    dew_point = properties_dewpoint_value;
    wind_speed = properties_windSpeed_value;
  }

  if(properties_temperature["value"] == nullptr)
  {
    Log.warning(F("No valid tempature received" CR));
  }
    String output;
    serializeJson(properties, output);
    output.concat(CR);
    Log.info(output.c_str());
  Log.info(F("Raw Message %s" CR), raw_message);
  Log.info(F("%s - Temp: %d C, Dew Point: %d C, Wind Speed: %d km/h" CR), description, temperature, dew_point,
           wind_speed);

  return CURRENT_CONDITIONS_OK;
}
