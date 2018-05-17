const _ = require('lodash');
const geolib = require('geolib');
const uuid = require('uuid/v4');
const rp = require('request-promise');

const LOCATION_SHANGHAI = {
  latitude: 31.2304,
  longitude: 121.4737,
};
const POINT_COUNT = 1000000;
const SCALE = 2 * 1000;

function generateLocation(center, scale) {
  const id = uuid();
  let loc = center;

  loc = geolib.computeDestinationPoint(loc, (Math.random() - 0.5) * scale, 0);
  loc = geolib.computeDestinationPoint(loc, (Math.random() - 0.5) * scale, 90);

  const { latitude, longitude } = loc;

  return [id, latitude, longitude];
}

const points = _.times(POINT_COUNT, () => generateLocation(LOCATION_SHANGHAI, SCALE));

console.log(`Generated ${points.length} points`);

rp.post('http://localhost:8000/create-index', { json: { points } }).then(result => {
  console.log(result);
});
