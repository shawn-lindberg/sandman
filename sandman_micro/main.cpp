#include "mbed.h"

// Constants
//

// Time between ticks.
#define TICK_INTERVAL_US   (5 * 1000 * 1000) // 5 sec.

// Maximum duration of the moving state.
#define MAX_MOVING_STATE_DURATION_US (100 * 1000 * 1000) // 100 sec.

// Maximum duration of the cool down state.
#define MAX_COOL_DOWN_STATE_DURATION_US (50 * 1000 * 1000) // 50 sec.

// Types
//

// Types of controls.
enum ControlTypes
{
    CONTROL_HEAD_UP = 0,
    CONTROL_HEAD_DOWN,
    CONTROL_KNEE_UP,
    CONTROL_KNEE_DOWN,
    CONTROL_ELEVATION_UP,
    CONTROL_ELEVATION_DOWN,

    NUM_CONTROL_TYPES,
};

// States a control may be in.
enum ControlState
{

    CONTROL_STATE_IDLE = 0,
    CONTROL_STATE_MOVING,
    CONTROL_STATE_COOL_DOWN,    // A delay after moving before moving can occur again.
};

// An individual control.
class Control
{
    public:
    
        // Handle initialization.
        //
        // p_PinOut:   The pin to output to. 
        //
        void Initialize(DigitalOut* p_PinOut);

        // Process a tick.
        //
        void Process();

        // Set moving desired for the next tick.
        //
        void MovingDesired();
        
    private:
    
        // The control state.
        ControlState m_State;
    
        // How many ticks until a state transition is forced.
        unsigned int m_TicksRemaining;
    
        // Whether movement is desired.
        bool m_MovingDesired;
        
        // The pin the control outputs to.
        DigitalOut* m_PinOut;
};

// Locals
//

// Serial connection over USB.
Serial s_PCSerial(USBTX, USBRX); // tx, rx

// Output pins.
DigitalOut s_Pin19Out(p19);
DigitalOut s_Pin20Out(p20);
DigitalOut s_Pin21Out(LED1);
DigitalOut s_Pin22Out(LED2);
DigitalOut s_Pin23Out(LED3);
DigitalOut s_Pin24Out(LED4);

DigitalOut* s_PinOuts[] =
{
    &s_Pin19Out,    // CONTROL_HEAD_UP
    &s_Pin20Out,    // CONTROL_HEAD_DOWN
    &s_Pin21Out,    // CONTROL_KNEE_UP
    &s_Pin22Out,    // CONTROL_KNEE_DOWN
    &s_Pin23Out,    // CONTROL_ELEVATION_UP
    &s_Pin24Out,    // CONTROL_ELEVATION_DOWN    
};

// The controls.
static Control s_Controls[NUM_CONTROL_TYPES];

// Ticker that handles state updates.
static Ticker s_Ticker;

// Functions
//

// Control members

// Handle initialization.
//
// p_PinOut:   The pin to output to. 
//
void Control::Initialize(DigitalOut* p_PinOut)
{
    m_State = CONTROL_STATE_IDLE;
    m_TicksRemaining = 0;
    m_MovingDesired = false;
    m_PinOut = p_PinOut;
}

// Process a tick.
//
void Control::Process()
{
    // Handle state transitions.
    switch (m_State) 
    {
        case CONTROL_STATE_IDLE:
        {
            // Wait until moving is desired to transition.
            if (m_MovingDesired != true) {
                break;
            }
            
            // Transition to moving.
            m_State = CONTROL_STATE_MOVING;
            
            // Calculate how many ticks to stay in moving at maximum.
            m_TicksRemaining = MAX_MOVING_STATE_DURATION_US / TICK_INTERVAL_US;
            
            s_PCSerial.printf("Control @ 0x%x: State transition from idle to moving triggered.\n\r",
                reinterpret_cast<unsigned int>(this));           
        }
        break;
        
        case CONTROL_STATE_MOVING:
        {
            // Update ticks.
            m_TicksRemaining--;
            
            // Wait until moving isn't desired or the time limit has run out.
            if ((m_MovingDesired == true) && (m_TicksRemaining > 0))
            {
                break;
            }

            // Transition to cool down.
            m_State = CONTROL_STATE_COOL_DOWN;
            
            // Calculate how many ticks to stay in cool down at maximum.
            m_TicksRemaining = MAX_COOL_DOWN_STATE_DURATION_US / TICK_INTERVAL_US;
            
            s_PCSerial.printf("Control @ 0x%x: State transition from moving to cool down triggered.\n\r",
                reinterpret_cast<unsigned int>(this));                      
        }
        break;
        
        case CONTROL_STATE_COOL_DOWN:
        {
            // Reset time if moving desired is received.
            if (m_MovingDesired == true)
            {
                // Calculate how many ticks to stay in cool down at maximum.
                m_TicksRemaining = MAX_COOL_DOWN_STATE_DURATION_US / TICK_INTERVAL_US;

                s_PCSerial.printf("Control @ 0x%x: Cool down timer reset due to input.\n\r",
                    reinterpret_cast<unsigned int>(this));                                 
            }
            
            // Update ticks.
            m_TicksRemaining--;
            
            // Wait until the time limit has run out.
            if (m_TicksRemaining > 0)
            {
                break;
            }

            // Transition to idle.
            m_State = CONTROL_STATE_IDLE;
            
            s_PCSerial.printf("Control @ 0x%x: State transition from cool down to idle triggered.\n\r",
                reinterpret_cast<unsigned int>(this));                                 
        }
        break;
        
        default:
        {
            s_PCSerial.printf("Control @ 0x%x: Unrecognized state %d in Process()\n\r", m_State,
                reinterpret_cast<unsigned int>(this));                      
        }
        break;
    }
    
    // Clear moving desired.
    m_MovingDesired = false;
    
    // Manipulate pin based on state.
    if (m_PinOut == NULL)
    {
        return;
    }
    
    if (m_State == CONTROL_STATE_MOVING)
    {
        (*m_PinOut) = 1;
    }
    else
    {
        (*m_PinOut)= 0;
    }
}

// Set moving desired for the next tick.
//
void Control::MovingDesired()
{
    m_MovingDesired = true;
    
    s_PCSerial.printf("Control @ 0x%x: Setting move desired to true\n\r", reinterpret_cast<unsigned int>(this));                      
}

// Process controls.
//
void ProcessControls()
{
    // Process the controls.
    for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
    {
        s_Controls[l_ControlIndex].Process();
    }
}

int main() {

    // Initialize the controls.
    for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
    {
        s_Controls[l_ControlIndex].Initialize(s_PinOuts[l_ControlIndex]);
    }
    
    // Start the ticker.
    s_Ticker.attach_us(&ProcessControls, TICK_INTERVAL_US);
    
    while (1)
    {
    
        // Store a command here.
        unsigned int const l_CommandBufferSize = 8;
        char l_CommandBuffer[l_CommandBufferSize];
 
        // Get a command.
        unsigned int l_CommandLength = 0;
        
        // Get the first character.
        char l_NextChar = s_PCSerial.getc();
        
        // Accumulate characters until we get a terminating character or we run out of space.
        while ((l_NextChar != ' ') && (l_CommandLength < (l_CommandBufferSize - 1)))
        {
            l_CommandBuffer[l_CommandLength] = l_NextChar;
            l_CommandLength++;
            
            // Get the next character.
            l_NextChar = s_PCSerial.getc();
        }
       
        // Terminate the command.
        l_CommandBuffer[l_CommandLength] = '\0';
        
        // Echo the command back.
        s_PCSerial.printf("Received a command: \"%s\"\n\r", l_CommandBuffer);
        
        // Command handling.
        if (l_CommandBuffer[0] == 'h')
        {
            if (l_CommandBuffer[1] == 'u')
            {
                s_Controls[CONTROL_HEAD_UP].MovingDesired();
            }
            else if (l_CommandBuffer[1] == 'd')
            {
                s_Controls[CONTROL_HEAD_DOWN].MovingDesired();                
            }
        }
        else if (l_CommandBuffer[0] == 'k')
        {
            if (l_CommandBuffer[1] == 'u')
            {
                s_Controls[CONTROL_KNEE_UP].MovingDesired();
            }
            else if (l_CommandBuffer[1] == 'd')
            {
                s_Controls[CONTROL_KNEE_DOWN].MovingDesired();                
            }
        }
        else if (l_CommandBuffer[0] == 'e')
        {
            if (l_CommandBuffer[1] == 'u')
            {
                s_Controls[CONTROL_ELEVATION_UP].MovingDesired();
            }
            else if (l_CommandBuffer[1] == 'd')
            {
                s_Controls[CONTROL_ELEVATION_DOWN].MovingDesired();                
            }
        }
    }
}
