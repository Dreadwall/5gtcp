#ifndef NETSLICES_H
#define NETSLICES_H

enum flow_type { GBR, NonGBR };

#define NA 0

class NetSlice
{
private:
	// Fields taken from https://www.awardsolutions.com/portal/shareables/evolution-qos-classes-5g-charlie-martin
    flow_type type;
    float loss_rate; // 0.10 = 10% loss
    int max_burst;	 // In Kilobytes
    uint32_t average_window;  // In Kilobytes
    uint32_t minimum_window;  // In Kilobytes
    int average_timeout; // In micro seconds

    // int priority_level; excluded because this is for routing only
    // int packet_delay_budget; excluded because this is for routing only
 
public:
    NetSlice(flow_type type, float loss_rate, int max_burst, uint32_t average_window, 
    	uint32_t minimum_window, int average_timeout)
    {
        setSliceConfig(type, loss_rate, max_burst, average_window, minimum_window, average_timeout);
    }
 
    void setSliceConfig(flow_type t, float lr, int mb, uint32_t aw, uint32_t mw, int at)
    {
        type = t;
        loss_rate = lr;
        max_burst = mb;
        average_window = aw;
        minimum_window = mw;
        average_timeout = at;

    }
 	
 	flow_type getType() { return type; }
    float getLossRate() { return loss_rate; }
    int getMaxBurst() { return max_burst; }
    uint32_t getAverageWindow() { return average_window; }
    uint32_t getMinimumWindow() { return minimum_window; }
    int getAverageTimeout() { return average_timeout; }
};

#endif // NETSLICES_H



