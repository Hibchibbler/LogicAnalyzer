`include "LogicCapture.v"

module LogicCapureTB();


reg clk, resetn;

reg [31:0] control;
reg [31:0] config0;
reg [31:0] config1;

reg [7:0] datain;

initial
begin
    clk     <= 1'b0;
    resetn  <= 1'b0;
    control <= 32'd0;
    config0 <= 32'd0;
    config1 <= 32'd0;
    datain  <= 8'd0;
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
	.control(control), //reg1
	.config0(config0), //reg2
	.config1(config1), //reg3
	.datain(datain),   //From some Nexy4 Port
	.dataout(),        //to RAM
    .we(),
    .en(),
    .address()
);

initial
begin
    #(5)  resetn <= 1'b1;
    #(20) control[0] <= 1'b1;
    #(10) datain <= 8'd3;
    #(4)  datain <= 8'd1;
    #(2)  datain <= 8'd2;
    #(4)  datain <= 8'd1;
    #(4)  datain <= 8'd7;
    #(4)  datain <= 8'd123;
    #(10) datain <= 8'd1;
    #(2)  datain <= 8'd33;
    #(50)  $finish;
end


endmodule
