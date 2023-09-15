module max2
(
    input           clock,
    input   [3:0]   key,
    output  [7:0]   led,
    output          f0, f1, f2,
    output          f3, f4, f5,
    output          dp, dn, pt
);

wire [15:0] address;

assign led = out;
assign {dp, dn, pt} = {rd, we};
assign {f0, f1, f2, f3, f4, f5} = address[5:0] + address[11:6] + address[15:11];

core Core6502
(
    .clock      (clock),
    .ce         (1'b1),
    .reset_n    (key[0]),
    .address    (address),
    .in         ({key[3:0], ~key[3:0]}),
    .out        (out),
    .rd         (rd),
    .we         (we)
);

endmodule

`include "../core.v"