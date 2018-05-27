#include <iostream>
#include <memory>
#include <vector>
#include <utility>
#include <string>
#include <regex>

#include <Poco/JSON/Parser.h>
#include <Poco/URI.h>
#include <Poco/Redis/Type.h>
#include <Poco/Redis/Exception.h>

#include <s2/s1angle.h>
#include <s2/s2latlng.h>
#include <s2/s2earth.h>
#include <s2/s2closest_point_query.h>

#include "query-index-redis.h"

using Poco::JSON::Parser;
using Poco::JSON::Object;

const std::regex UUID_REGEX("[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}");
const double DEFAULT_RADIUS = 100.00;

void RedisQueryIndexRequestHandler::handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) {
  const Poco::URI uri(request.getURI());
  const std::string &path = uri.getPath();

#ifdef DEBUG
  std::cout << "Query index" << std::endl;
  std::cout << "URI: " << request.getURI() << std::endl;
#endif // DEBUG

  std::smatch matchUUID;
  std::ostream &ostm = response.send();

  if (std::regex_search(path, matchUUID, UUID_REGEX)) {

    // Process the query parameters
    const Poco::URI::QueryParameters query = uri.getQueryParameters();
    const std::unordered_map<std::string, std::string> params(query.begin(), query.end());

    const auto pLatitude = params.find("latitude");
    const auto pLongitude = params.find("longitude");
    const auto pCount = params.find("count");
    const auto pRadius = params.find("radius");

    if (pLatitude == params.end() || pLongitude == params.end() || pCount == params.end()) {
      response.setStatus(HTTPServerResponse::HTTP_BAD_REQUEST);
      ostm << HTTPServerResponse::HTTP_REASON_BAD_REQUEST;
      return;
    }


    const double latitude = std::stod(pLatitude -> second);
    const double longitude = std::stod(pLongitude -> second);
    const int count = std::stoi(pCount -> second);
    const double radius = pRadius == params.end() ? DEFAULT_RADIUS : std::stod(pRadius->second) / 1000;

#ifdef DEBUG
    std::cout << "Latitude: " << latitude << std::endl;
    std::cout << "Longitude: " << longitude << std::endl;
    std::cout << "Count: " << count << std::endl;
    std::cout << "Radius: " << radius << std::endl;
#endif // DEBUG

    Poco::Redis::Array cmd;
    // cmd << "NEARBY" << matchUUID.str() << "LIMIT" << std::to_string(count) << "POINT" << std::to_string(latitude) << std::to_string(longitude);
    cmd << "NEARBY" << "18105e3a-6180-11e8-882c-000d3aa0926a" << "LIMIT" << "100" << "POINT" << "31.2304" << "121.473";
    std::cout << cmd.toString() << std::endl;
    try{
      auto ret = m_redisClient->execute<std::string>(cmd);
    }catch(RedisException e){
      std::cout << e.what() << std::endl;
    }
    
    // Build the query
//    auto queryResult = knnQuery(matchUUID.str(), std::to_string(longitude), std::to_string(latitude), count , radius);
    
//    const int size = queryResult.size();

    response.setContentType("text/json");

    // Generate the output
    Poco::JSON::Object result;
    result.set("id", matchUUID.str());
    result.set("count", count);
    for (auto &pair: m_performanceLogger) {
      result.set(pair.first, pair.second);
    }

    std::cout << "Here0" << std::endl;

    Parser parser;
    Poco::Dynamic::Var parsedResult = parser.parse(ret);
    Poco::JSON::Object::Ptr object = parsedResult.extract<Object::Ptr>();
    Poco::JSON::Array::Ptr jsonPoints = object->getArray("objects");

    std::cout << "Here1" << std::endl;
    Poco::JSON::Array points;
    for(int i=0; i<(int)jsonPoints->size(); ++i) {
      auto p = jsonPoints->getObject(i);
      Poco::JSON::Object point;
      std::cout << "Here2" << std::endl;

      std::string pid = p->getValue<std::string>("id");
      point.set("id", pid);
      std::cout << "Here3" << std::endl;
      
      Poco::Dynamic::Var subObject = p->get("object");
      std::cout << "Here4" << std::endl;
      Object::Ptr coords = subObject.extract<Object::Ptr>();
      std::cout << "Here5" << std::endl;
      Poco::JSON::Array::Ptr theCoords = coords->getArray("coordinates");
      std::cout << "Here6" << std::endl;

      point.set("latitude", theCoords->get(1));
      point.set("longitude", theCoords->get(0));
      points.add(point);
    }
    result.set("points", points);

#ifdef DEBUG
    result.stringify(std::cout);
#endif // DEBUG
    result.stringify(ostm);
  }
}

std::vector<GeoPoint> RedisQueryIndexRequestHandler::knnQuery(const std::string &indexId, const std::string &lng, const std::string &lat, int n, const double &maxRadius)
{
  double currectRadius = 0.1;
  Poco::Redis::Array result;
  do {
    Poco::Redis::Array cmd;
    cmd << "georadius" << indexId << lng << lat << std::to_string(currectRadius) << "km" << "ASC" << "WITHCOORD";
    result = m_redisClient->execute<Poco::Redis::Array>(cmd);
    currectRadius = currectRadius * 2;
  } while(result.size() < n && currectRadius < maxRadius);

#ifdef DEBUG
  std::cout << "In function RedisQueryIndexRequestHandler::knnQuery()" << std::endl;
  std::cout << "Retrive " << result.size() << " cars." << std::endl;
#endif // DEBUG
 
  std::vector<GeoPoint> ret(n);
  GeoPoint point;
  for(int i=0; i<n && i<(int)result.size(); ++i){
    auto item = result.get<Poco::Redis::Array>(i);
    auto carId = item.get<Poco::Redis::BulkString>(0).value();
    auto coord = item.get<Poco::Redis::Array>(1);
    auto lng = coord.get<Poco::Redis::BulkString>(0).value();
    auto lat = coord.get<Poco::Redis::BulkString>(1).value();
    point.first = carId;
    point.second = std::make_pair(std::stod(lng), std::stod(lat));
    ret[i] = point;
#ifdef DEBUG
    std::cout << "Car: " << carId << "(" << lng << ", " << lat << ")" << std::endl;
#endif // DEBUG
  }

  return ret;
}
