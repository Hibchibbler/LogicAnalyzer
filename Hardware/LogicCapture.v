module LogicCapture(
    input wire        clk,
	input wire        resetn,
	
	output reg [31:0] status,  //reg0
	input  wire[31:0] control, //reg1
	input  wire[31:0] config0, //reg2
	input  wire[31:0] config1, //reg3
	
	input  wire[7:0] datain,//From some Nexy4 Port
	output reg [7:0] dataout,//to RAM
	                         //TODO: more signals to interface to RAM...
    output reg we,
    output reg en,
    output reg[17:0] address
	);

	reg [17:0] BRAM_WR_Addr;
	reg [7:0] data_in_reg;
    reg [7:0] data_in_reg_prev;
    
    reg [7:0] falling_edges;
    reg [7:0] rising_edges;

    reg started;
    
    reg state;//0 sampling, 1 writing to BRAM
	
	//--------------//
    //     RESET    // DONE
    //--------------//
	always @(posedge clk or negedge resetn)
        begin
            if(resetn == 1'b0) begin
                status           <= 0; //clear status register 
                data_in_reg      <= 0;
                data_in_reg_prev <= 0;
                BRAM_WR_Addr     <= 0;
                started          <= 0;
                state            <= 1'b0;
                we               <= 1'b0;
                en               <= 1'b0;
                address          <= 18'd0;
                dataout          <= 8'd0;
                rising_edges     <= 8'd0;
                falling_edges    <= 8'd0;
            end else begin
                //Store last sample, and get new one for comparison.
                data_in_reg_prev = data_in_reg;
                data_in_reg      = datain;
            
                if (control[0] && !control[1]) begin
                    started   = 1;
                    status[0] = 1'b1;
                end 
                else begin
                    started   = 0;
                    status[0] = 1'b0;                    
                end
                
                if(started ==1) begin //We are good to go
                    //The "main" state - sample. if transition, write to BRAM.
                    if (state == 1'b0) begin
                        //Compare previous sample to current sample and
                        //record if there is a change
                        if (data_in_reg_prev ^ data_in_reg) begin
                            address      = BRAM_WR_Addr;
                            dataout      = datain;
                            en           = 1;
                            we           = 1;
                            BRAM_WR_Addr <= BRAM_WR_Addr + 1'b1;
                            state        <= 1'b1;
                        end else
                        begin
                            en           <= 0;
                            we           <= 0;
                            address      <= address;
                            dataout      <= dataout;
                            BRAM_WR_Addr <= BRAM_WR_Addr;
                            state        <= 1'b0;
                        end
                        //Determine which channels falling & rising edges happened on
                        falling_edges = (~data_in_reg & data_in_reg_prev);
                        rising_edges  = (data_in_reg & ~data_in_reg_prev);
                        //Check if buffer is full
                        if (BRAM_WR_Addr == 18'd262143) begin
                           status[0]    <= 1'b0;
                           started      <= 0;                                
                           BRAM_WR_Addr <= 0;
                        end
                    end else
                    //Deassert "en/we" state
                    //This state exists so that if one clock cycle (10 ns), is to quick too deassert the en/we lines
                    //we can easily adjust it by changing how or when we get to this state.
                    if (state == 1'b1) begin
                        en            <= 0;
                        we            <= 0;
                        state         <= 1'b0;
                        falling_edges <= 8'd0;
                        rising_edges  <= 8'd0;
                    end
                end
            end   
        end

endmodule
