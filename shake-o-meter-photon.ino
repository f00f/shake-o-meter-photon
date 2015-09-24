// ------------
//  Particle photon firmware to detect vibrations of a laundry machine - and when it's finished.
//  License: MIT
//  Github: https://github.com/f00f/shake-o-meter-photon
// ------------

// Configure pins
const int led1 = D0;
const int sensorPower = D4;// pin outs have a more stable voltage than the regular power supply
const int sensor = A0;

//! sampling delay in [ms]
const int sampling_delay = 1500;

//! size of the sliding window in number of samples
const int window_size = 12;
//! number of positive samples in window which are required to detect spinning state.
const int num_positive_samples_in_window_thres = 3;

//! sensor reading when in rest
const int sensor_idle_value = 2047;
//! threshold on amplitude to detect vibration
const int amp_threshold = 100;
// wait time in [seconds] after spinning has stopped, before considering finished
const int wait_time = 5;


const int S_INVALID = 0;
const int S_IDLE = 1;
const int S_SPINNING = 2;
const int S_WAITING = 3;
const int S_FINISHED = 4;

const char* spinning_pub_string = "SPINNING";
const char* waiting_pub_string = "WAITING";
const char* finished_pub_string = "FINISHED";
const char* idle_pub_string = "IDLE";

bool samples[window_size];
int sample_pos = 0;

int state;
unsigned int wait_timeout;


inline int ReadSensor();
inline void PublishSample(int amp);
inline void StoreSample(int amp);
inline bool AnalyzeWindow();
inline void PublishStateChange(const char* newState);


void setup() {
    pinMode(led1, OUTPUT);
    pinMode(sensorPower, OUTPUT);
    pinMode(sensor, INPUT);
    
    // enable power for the sensor (pin outs are more stable than the regular power supply)
    digitalWrite(sensorPower, HIGH);

    // read some values until sensor has stabilized
    delay(200);
    for (int i = 0; i < 5; i++) {
        analogRead(sensor);
        delay(200);
    }

    state = S_IDLE;
    wait_timeout = 0;
}


void loop() {
    int amp = ReadSensor();

    PublishSample(amp);

    StoreSample(amp);

    bool is_vibrating = AnalyzeWindow();

    // State machine
    switch (state) {
        case S_IDLE:
            if (is_vibrating) {
                // Spinning has started
                digitalWrite(led1, HIGH);
                state = S_SPINNING;
                PublishStateChange(spinning_pub_string);
            }
            break;
        case S_SPINNING:
            if (!is_vibrating) {
                // Spinning might have finished
                state = S_WAITING;
                wait_timeout = millis() + 1000 * wait_time;
                PublishStateChange(waiting_pub_string);
            }
            break;
        case S_WAITING:
            if (is_vibrating) {
                // Spinning has re-started
                wait_timeout = 0;
                digitalWrite(led1, HIGH);
                state = S_SPINNING;
                PublishStateChange(spinning_pub_string);
                break;
            } else if (millis() > wait_timeout) {
                // Finished
                wait_timeout = 0;
                digitalWrite(led1, LOW);
                state = S_FINISHED;
                PublishStateChange(finished_pub_string);
            }
            break;
        case S_FINISHED:
            // Reset for next run
            state = S_IDLE;
            PublishStateChange(idle_pub_string);
            break;
    }

    // We'll leave it on for 1 second...
    delay(sampling_delay);

    // And repeat!
}

inline int ReadSensor() {
    int sensor_value = analogRead(sensor);
    return abs(sensor_idle_value - sensor_value);
}

inline void PublishSample(int amp) {
    char amp_pub_string[5];
    sprintf(amp_pub_string, "%d", amp);
    Spark.publish("amplitude", amp_pub_string);
}

inline void StoreSample(int amp) {
    bool is_vibrating_now = (amp > amp_threshold);
    samples[sample_pos++] = is_vibrating_now;
    if (sample_pos > window_size) {
        sample_pos = 0;
    }
}

inline bool AnalyzeWindow() {
    // This can be optimized to just remove the oldest and add the new value.
    int num_pos_samp = 0;
    for (int i = 0; i < window_size; i++) {
        if (samples[i]) {
            num_pos_samp++;
        }
    }
    return (num_pos_samp > num_positive_samples_in_window_thres);
}

inline void PublishStateChange(const char* newState) {
    Spark.publish("State_Changed", newState);
}
