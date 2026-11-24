`timescale 1ns/1ps
// Simple SDR SDRAM model for SAROO memhub simulation.
// 4 banks, 13-bit row, 10-bit column, x16 (the 512Mbit / 64MB geometry).
// CL and BL fixed to what tsdram programs (CL=2, BL=4 sequential reads,
// single-location writes). Sparse storage; unwritten words read as init_val().
// Checks basic protocol/timing rules and counts violations.
// Output timing (tAC) is chosen so the ORIGINAL design captures read data
// correctly; it is not the K4S511632D datasheet timing.
module sdram_model(
	input clk,
	input cke,
	input cs_n, input ras_n, input cas_n, input we_n,
	input [12:0] a,
	input [1:0] ba,
	input [1:0] dqm,
	inout [15:0] dq
);
	parameter int  CL    = 2;
	parameter int  BL    = 4;
	parameter real TCK   = 10.0;
	parameter real T_AC  = 4.0;	// clock to data out
	parameter real T_RCD = 19.9;
	parameter real T_RP  = 19.9;
	parameter real T_WR  = 14.9;
	parameter real T_RFC = 60.0;

	logic [15:0] mem [int];
	logic [12:0] open_row [4];
	bit          row_open [4];
	realtime     bank_ready [4];
	realtime     act_time [4];
	realtime     last_ref = 0, max_ref_gap = 0;
	int          errors = 0, refs = 0, reads = 0, writes = 0;
	bit          seen_ref = 0;

	logic [15:0] dq_out;
	bit          dq_oe = 0;
	assign dq = dq_oe ? dq_out : 16'hzzzz;

	function automatic logic [15:0] init_val(int idx);
		logic [31:0] h;
		h = idx * 32'h9E3779B1;
		return h[31:16] ^ h[15:0];
	endfunction

	function automatic int index(logic [1:0] b, logic [12:0] row, logic [9:0] col);
		return {b, row[9:0], col};
	endfunction

	function automatic logic [15:0] rd_word(int idx);
		return mem.exists(idx) ? mem[idx] : init_val(idx);
	endfunction

	task automatic err(string s);
		errors++;
		if (errors <= 20) $display("%t SDRAM ERROR: %s", $realtime, s);
	endtask

	initial begin
		for (int i = 0; i < 4; i++) begin row_open[i] = 0; bank_ready[i] = 0; act_time[i] = 0; end
	end

	// one burst output process per READ
	task automatic do_burst(int b, logic [12:0] row, logic [9:0] col);
		for (int k = 0; k < BL; k++) begin
			logic [9:0] c;
			c = {col[9:2], col[1:0] + k[1:0]};
			#((k == 0) ? (CL * TCK + T_AC) : TCK);
			dq_out = rd_word(index(b, row, c));
			dq_oe = 1;
		end
		#(TCK);
		dq_oe = 0;
	endtask

	always @(posedge clk) if (cke && !cs_n) begin
		realtime t;
		t = $realtime;
		case ({ras_n, cas_n, we_n})
		3'b011: begin	// ACTIVE
			if (row_open[ba]) err($sformatf("ACT to open bank %0d", ba));
			if (t < bank_ready[ba]) err($sformatf("ACT bank %0d %.1fns early (tRP/tWR)", ba, bank_ready[ba] - t));
			if (seen_ref && t - last_ref < T_RFC) err($sformatf("ACT %.1fns after REF (tRFC)", t - last_ref));
			row_open[ba] = 1; open_row[ba] = a; act_time[ba] = t;
		end
		3'b101: begin	// READ
			if (!row_open[ba]) err($sformatf("READ to closed bank %0d", ba));
			else if (t - act_time[ba] < T_RCD) err($sformatf("READ %.1fns after ACT (tRCD)", t - act_time[ba]));
			reads++;
			fork do_burst(ba, open_row[ba], a[9:0]); join_none
			if (a[10]) begin row_open[ba] = 0; bank_ready[ba] = t + BL * TCK + T_RP; end
		end
		3'b100: begin	// WRITE (single location)
			int idx;
			logic [15:0] w;
			if (!row_open[ba]) err($sformatf("WRITE to closed bank %0d", ba));
			else if (t - act_time[ba] < T_RCD) err($sformatf("WRITE %.1fns after ACT (tRCD)", t - act_time[ba]));
			if (dq_oe) err("WRITE while SDRAM drives DQ");
			writes++;
			idx = index(ba, open_row[ba], a[9:0]);
			w = rd_word(idx);
			if (!dqm[1]) w[15:8] = dq[15:8];
			if (!dqm[0]) w[ 7:0] = dq[ 7:0];
			mem[idx] = w;
			if (a[10]) begin row_open[ba] = 0; bank_ready[ba] = t + T_WR + T_RP; end
		end
		3'b010: begin	// PRECHARGE
			for (int i = 0; i < 4; i++) if (a[10] || ba == i) begin row_open[i] = 0; bank_ready[i] = t + T_RP; end
		end
		3'b001: begin	// AUTO REFRESH
			for (int i = 0; i < 4; i++) begin
				if (row_open[i]) err($sformatf("REF with bank %0d open", i));
				if (t < bank_ready[i]) err($sformatf("REF %.1fns early (bank %0d precharge)", bank_ready[i] - t, i));
			end
			if (seen_ref && t - last_ref > max_ref_gap) max_ref_gap = t - last_ref;
			last_ref = t; seen_ref = 1; refs++;
		end
		3'b000: ;	// MODE REGISTER SET
		default: ;	// NOP
		endcase
	end
endmodule
