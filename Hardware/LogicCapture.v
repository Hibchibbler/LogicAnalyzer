module LogicCapture(
    input wire        clk,
	input wire        resetn,
	
	output reg [31:0] status,  //bit 0 -  busy, bit 1 - trigger detected, bits[19:2] trigger address
                               //bit 20 - pre trigger samples achieved
    output reg [31:0] status1, //stores the address of the final sample
	input  wire[31:0] control, //bit 0 - start, bit 1 - abort
	input  wire[31:0] config0, //trigger definition
	input  wire[31:0] config1, //{postTriggerSamples, preTriggerSamples}
	
	input  wire[7:0] datain,   //From some Nexy4 Port
	output reg [7:0] dataout,  //to BRAM
    output reg we,
    output reg en,
    output reg[17:0] address
	);

	reg [17:0] BRAM_WR_Addr;
	reg [7:0] data_in_reg;
    reg [7:0] data_in_reg_prev;
    
    wire [7:0] falling_edges;
    wire [7:0] rising_edges;
    reg [2:0] trigger_channel;
    reg RorF;      // Rising or falling 1 is rising, 0 is falling
    reg EDGE_TRIGGER_DISABLE; // bit 5 of the trigger definition register
    wire [7:0] TRIG; // Channel trigger hits
    reg TRIGGER;   // All channels matched global trigger signal
    
    
    reg [17:0] preTriggerSamples;
    reg [17:0] postTriggerSamples;

    reg preTriggerSamplesMet; // If true, indicates that at least preTriggerSamples total has been taken
    reg [17:0] postTriggerCtr;
    
    reg started;
    
    reg state; //0 sampling, 1 writing to BRAM
    
    wire [7:0] TRIG_VAL_ENABLE;
    wire [7:0] TRIG_VALUE_CMP;
    //  Do some of the combinational logic outside of the sequential block
    assign TRIG_VAL_ENABLE = {config0[31],config0[29],config0[27],config0[25],config0[23],config0[21],config0[19],config0[17]};
    assign TRIG_VALUE_CMP  = {config0[30],config0[28],config0[26],config0[24],config0[22],config0[20],config0[18],config0[16]}; 
    assign TRIG    = (data_in_reg ^~ TRIG_VALUE_CMP | ~TRIG_VAL_ENABLE) & ~{8{TRIGGER}};
    assign falling_edges    = (~data_in_reg & data_in_reg_prev);
    assign rising_edges     = (data_in_reg & ~data_in_reg_prev);
	
	//--------------//
    //     RESET    // DONE
    //--------------//
	always @(posedge clk or negedge resetn)
        begin
            if(resetn == 1'b0) begin
                status             <= 32'd0; //clear status register
                status1            <= 32'd0;
                data_in_reg        <= 8'd0;
                data_in_reg_prev   <= 8'd0;
                BRAM_WR_Addr       <= 19'd0;
                started            <= 1'b0;
                state              <= 1'b0;
                we                 <= 1'b0;
                en                 <= 1'b0;
                address            <= 18'd0;
                dataout            <= 8'd0;
                TRIGGER            <= 1'b0;
                RorF               <= 1'b0;
                trigger_channel    <= 3'd0;
                preTriggerSamples  <= 18'd0;
                postTriggerSamples <= 18'd0;
                postTriggerCtr     <= 18'd0;
                preTriggerSamplesMet <= 1'b0;
                EDGE_TRIGGER_DISABLE <= 1'b0;
            end else begin
                //Store last sample, and get new one for comparison.
                data_in_reg_prev <= data_in_reg;
                data_in_reg      <= datain;
            
                if (control[0]) begin
                    preTriggerSamples    <= {2'b00, config1[15:0]};   // Number of samples to take pre-trigger configured by user
                    postTriggerSamples   <= {2'b00, config1[31:16]};   // after trigger samples are whatever is left
                    started              <= 1'b1;
                    status[0]            <= 1'b1;
                    trigger_channel      <= config0[2:0];
                    RorF                 <= config0[3];
                    EDGE_TRIGGER_DISABLE <= config0[4];
                end

                if (control[1]) begin
                    started   <= 0;
                    status[0] <= 1'b0;
                end
                
                if(started) begin //We are good to go
                    //The "main" state - sample. if transition, write to BRAM.
                    if (state == 1'b0) begin
                        //Compare previous sample to current sample and
                        //record if there is a change
                        if (data_in_reg_prev ^ data_in_reg) begin
                            address      <= BRAM_WR_Addr[17:0];
                            dataout      <= datain;
                            en           <= 1'b1;
                            we           <= 1'b1;
                            BRAM_WR_Addr <= BRAM_WR_Addr + 19'd1;
                            state        <= 1'b1;
                            if (TRIGGER) begin
                                // Need to keep count of samples
                                // after the trigger to figure out when it stops
                                postTriggerCtr <= postTriggerCtr + 18'd1;
                            end
                            if (((BRAM_WR_Addr) == preTriggerSamples) & ~TRIGGER)
                            begin
                                preTriggerSamplesMet <= 1'b1;
                                status[20]           <= 1'b1; // Updates a status bit that at least 'preTrigger' samples taken
                            end
                        end else
                        begin
                            en           <= 1'b0;
                            we           <= 1'b0;
                            address      <= address;
                            dataout      <= dataout;
                            BRAM_WR_Addr <= BRAM_WR_Addr;
                            state        <= 1'b0;
                        end

                        //Trigger detection
                        if( ((rising_edges[trigger_channel] & RorF) | (falling_edges[trigger_channel] & ~RorF)) | EDGE_TRIGGER_DISABLE ) begin
                            //If here - a edge trigger occurred (or edge triggers are disabled)
                            if(TRIG == 8'hFF) begin
                                // if here - all required values matched on an edge trigger event
                                TRIGGER           <= 1;	
                                status[19:2]      <= BRAM_WR_Addr; // Update status register with value of trigger address
                                status[1]         <= 1'b1; // Update status bit that indicates trigger detected
                            end
                        end
                        
                        // If the post trigger counter(plus1) equals the posttrigger samples, quit
                        if ((postTriggerCtr + 1) == postTriggerSamples) begin
                            started   <= 1'b0;
                            status[0] <= 1'b0;
                        end
                    end else
                    //Deassert "en/we" state
                    //This state exists so that if one clock cycle (10 ns), is to quick too deassert the en/we lines
                    //we can easily adjust it by changing how or when we get to this state.
                    if (state == 1'b1) begin
                        en            <= 1'b0;
                        we            <= 1'b0;
                        state         <= 1'b0;
                    end
                end else
                begin
                    en   <= 1'b0;
                    we   <= 1'b0;
                    // If the start bit is 0, just set the 
                    // "last address sampled" to the current address.
                    status1[17:0] <= address;
                end
            end
        end

endmodule
