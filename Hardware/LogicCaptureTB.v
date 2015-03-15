`include "LogicCapture.v"

module LogicCapureTB();


reg clk, resetn;

reg test2;

reg [31:0] control;
reg [31:0] config0;
reg [31:0] config1;
reg [31:0] config2;

reg [7:0] datain;

wire [2:0] channelNumber;
wire       riseNotFall;
wire [1:0] ch0;
wire [1:0] ch1;
wire [1:0] ch2;
wire [1:0] ch3;
wire [1:0] ch4;
wire [1:0] ch5;
wire [1:0] ch6;
wire [1:0] ch7;

wire [17:0] preTrigger;
wire [17:0] postTrigger;

initial
begin
    test2   <= 1'b0;
    clk     <= 1'b0;
    resetn  <= 1'b0;
    control <= 32'd0;
    config0 <= 32'd0;
    config1 <= 32'd0;
    datain  <= 8'd0;
    config2 <= 32'd0;
    $dumpfile("simresults.vcd");
    $dumpvars;
end

always
begin
 #(1) clk = ~clk;
end

LogicCapture captureModule(
    .clk(clk),
	.resetn(resetn),
	.status(),         //reg0
    .status1(),
	.control(control), //reg1
	.config0(config0), //reg2
	.config1(config1), //reg3
	.datain(datain),   //From some Nexy4 Port
	.dataout(),        //to RAM
    .we(),
    .en(),
    .address()
);

assign channelNumber = 3'd0;
assign riseNotFall = 1'b1;

assign preTrigger = 16'h0005;
assign postTrigger = 16'h000A;

assign ch0 = 2'b00;
assign ch1 = 2'b10;
assign ch2 = 2'b00;
assign ch3 = 2'b11;
assign ch4 = 2'b00;
assign ch5 = 2'b00;
assign ch6 = 2'b00;
assign ch7 = 2'b00;

initial
begin
    #(5)  resetn     <= 1'b1;
    #(4)  config1[15:0]    <= preTrigger;
          config1[31:16]   <= postTrigger;
          config0    <= {ch7,ch6,ch5,ch4,ch3,ch2,ch1,ch0,12'b000000000000, riseNotFall, channelNumber};
    #(20) control[0] <= 1'b1;
    #(8)  datain[0]  <= 1'b1;
          control[0] <= 1'b0;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
          datain[3]  <= 1'b1;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
    #(8)  datain[0]  <= 1'b0;
    #(8)  datain[0]  <= 1'b1;
            cntr <= 31'd0;
    #(8)  datain[0]  <= 1'b0;
    #(4)   test2      <= 1'b1;
//    #(4)  control[1] <= 1'b1
    #(50)  $finish;
end

reg [31:0] cntr;

always @(posedge clk)
begin
if (test2)
begin
    if (~resetn)
    begin
        cntr <= 32'd0;
    end
    else
    begin
        if (cntr > 32'd200)
            $finish;
        else;
          begin
            datain[0] <= ~datain[0];
            cntr <= cntr + 31'd1;
          end
    end
end
end



endmodule
