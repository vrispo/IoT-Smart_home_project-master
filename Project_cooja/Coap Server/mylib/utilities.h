#ifndef UTILITIES_LIB_H_
#define UTILITIES_LIB_H_

/* Client configuration */
#define SERVER_CLOUD  "coap://[fd00::1]:5683"
#define NOT_FOUND     "not_found"
#define NO_ROOM       "no_room"
#define NO_TYPE       "no_type"
#define MAX_ROOM_DIM  20
#define MAX_SENSORS   20
#define RESPONSE_BAD   0
#define RESPONSE_OK   1

/* CLOUD conf */
#define REGISTRATION_SERVICE_URL "/registration"
#define SENSOR_IP_REQUEST_URL    "/request"

/* URI Resources */
#define ALARM_ACT_URL           "/alarm_act"
#define ALARM_SENS_URL          "/alarm_s"
#define LIGHT_BULB_URL          "/light_bulb"
#define LIGHT_SENS_URL          "/light_s"
#define PRES_SENS_URL           "/presence_s"
#define ROLL_BL_ACT_URL         "/roller_blind"
#define WASH_ACT_URL            "/washing_m"
#define HVAC_URL                "/hvac"
#define TEMP_HUM_URL            "/temp_hum"

/* TYPE sens-act*/

#define ALARM_ACT_TYPE           "alarm actuator"
#define ALARM_SENS_TYPE          "alarm sensor"
#define LIGHT_BULB_TYPE          "light bulb"
#define LIGHT_SENS_TYPE          "light sensor"
#define PRES_SENS_TYPE           "presence sensor"
#define ROLL_BL_ACT_TYPE         "roller blind"
#define WASH_ACT_TYPE            "washing machine"
#define HVAC_TYPE                "hvac"
#define TEMP_HUM_TYPE            "temperature_humidity sensor"


/* OPERATIONS sens-act */

#define ALARM_ACT_OPERATIONS            "GET/PUT/POST"
#define ALARM_SENS_OPERATIONS           "GET"
#define LIGHT_BULB_OPERATIONS           "GET/PUT/POST"
#define LIGHT_SENS_OPERATIONS           "GET"
#define PRES_SENS_OPERATIONS            "GET"
#define ROLL_BL_ACT_OPERATIONS          "GET/PUT/POST"
#define WASH_ACT_OPERATIONS             "GET/PUT/POST"
#define HVAC_OPERATIONS                 "GET/PUT/POST"
#define TEMP_HUM_OPERATIONS             "GET"
#define NONE

/* POST-GET FORMAT sens-act */

#define NONE_FORMAT					 "none"

#define ALARM_ACT_PFORMAT            "mode=on|off"
#define LIGHT_BULB_PFORMAT           "mode=on|auto|off&brightness=0..100&color=y|g|r"
#define ROLL_BL_ACT_PFORMAT          "mode=on|off&level=0..100"
#define WASH_ACT_PFORMAT             "mode=start|pause&program=0|1|2&centrifuge=0|1|2"
#define HVAC_PFORMAT                 "mode=on|off&temp=-10..40&hum=0..100"


#define HVAC_GFORMAT                 "value=temp&|hum"
#define TEMP_HUM_GFORMAT             "type=temp&|hum"

#endif /* UTILITIES_LIB_H_ */
