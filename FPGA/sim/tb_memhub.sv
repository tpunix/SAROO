`timescale 1ns/1ps
// Testbench: SAROO memhub + tsdram + cachebus + cacheblk against an SDRAM model,
// driven by a model of the Saturn SCU on the A-Bus (CS0, 4MB RAM cart area)
// and random STM32 traffic on port B.
//
// The A-Bus front end (synchroniser, start pulses, address register, byte
// swap) copies SSMaster.v lines 447-472 and 685-707.
//
// SCU cycle model (approximation, not from Sega's timing diagrams):
//   CS0 + RD/WR asserted on an SCU clock edge, NW wait clocks, then if EWT=1
//   extra clocks while WAIT is asserted, then data is sampled on the next
//   edge and the strobes are released. RPC=1 adds one idle clock after.
//   SCU clock 28.636MHz (34.92ns), asynchronous to the FPGA's 100MHz.
//
// Parameters (override with vsim -G):
//   NW, EWT, RPC   SCU timing (Saroo 0x3880: NW=8 EWT=1; Sega 0x2330: NW=3 EWT=0)
//   B_LOAD         STM32 traffic: 0 none, 1 light, 2 heavy
//   SIM_US         simulated time after SDRAM init, in microseconds
//   SEED           random seed
module tb;
	parameter int NW     = 8;
	parameter int EWT    = 1;
	parameter int RPC    = 1;	// precharge clock after reads
	parameter int WPC    = 0;	// precharge clock after writes
	parameter int B_LOAD = 0;
	parameter int SAT_IDLE_PCT = 0;	// % of Saturn accesses followed by a 0.5-3us pause
	parameter int SIM_US = 1000;
	parameter int TRAFFIC = 0;	// 0: mixed, 1: random 32-bit write/read/read (stress test 5), 2: sequential 32-bit reads
	parameter real SAMPLE_ADV = 0.0;	// ns the SCU samples read data before the end of the cycle
	parameter real WDATA_DELAY = 0.0;	// ns after the start of a write cycle until the new data is valid at the FPGA
	parameter int SEED   = 1;
	parameter string NAME = "run";

	localparam real TSCU = 34.92;

	// clocks / reset
	logic clk = 0;  always #5 clk = ~clk;			// mclk 100MHz
	logic sdclk;    assign sdclk = ~clk;			// SD_CLK: 180 deg (mainpll clk1)
	logic sclk = 0; initial begin #3.3; forever #(TSCU/2) sclk = ~sclk; end
	logic reset = 0; initial #100 reset = 1;		// active-low

	// ---------------- Saturn A-Bus (CS0) ----------------
	logic        SS_CS0n = 1, SS_RDn = 1, SS_WR0n = 1, SS_WR1n = 1;
	logic [23:0] SS_ADDR = 0;
	logic [15:0] scu_drv = 0;
	logic        scu_drv_en = 0;
	wire  [15:0] SS_DATA;

	logic s0 = 1, s1 = 1, s2 = 1, s3 = 1, s4 = 1;
	always @(posedge clk) begin s0 <= SS_CS0n; s1 <= s0; s2 <= s1; s3 <= s2; s4 <= s3; end
	wire ss_rd_start = (s2 == 1 && s1 == 0 && SS_RDn == 0);
	wire ss_wr_start = (s4 == 1 && s3 == 0 && (SS_WR0n == 0 || SS_WR1n == 0));

	logic [25:0] ss_ram_addr;
	always @(posedge clk) ss_ram_addr <= {2'b00, SS_ADDR};
	wire        ss_ram_cs  = ~SS_CS0n;
	wire [ 1:0] ss_mask    = {SS_WR0n, SS_WR1n};
	wire [15:0] ss_ram_din = {SS_DATA[7:0], SS_DATA[15:8]};
	wire [15:0] ss_ram_dout;
	wire        ss_ram_wait;
	wire [15:0] ss_ram_data_out = {ss_ram_dout[7:0], ss_ram_dout[15:8]};
	assign SS_DATA = scu_drv_en ? scu_drv : (SS_RDn == 0 && SS_CS0n == 0) ? ss_ram_data_out : 16'hzzzz;

	// ---------------- STM32 port ----------------
	logic        st_cs = 0, st_rd = 0, st_wr = 0;
	logic [ 1:0] st_mask = 0;
	logic [25:0] st_addr = 0;
	logic [15:0] st_din = 0;
	wire  [15:0] st_dout;
	wire         st_wait;

	// ---------------- refresh / config ----------------
	logic ext_refresh = 0;
	wire [11:0] reg_sdram = 12'b11_1100001101;	// 64MB, refresh count 0x30d (7.81us)

	// ---------------- DUT ----------------
	wire sd_cke, sd_cs, sd_ras, sd_cas, sd_we;
	wire [12:0] sd_addr;
	wire [ 1:0] sd_ba, sd_dqm;
	wire [15:0] sd_data;

	memhub u_hub(
		.reset(reset), .clk(clk),
		.cs_a(ss_ram_cs), .rd_a(ss_rd_start), .wr_a(ss_wr_start), .mask_a(ss_mask), .nwait_a(ss_ram_wait),
		.addr_a(ss_ram_addr), .wdata_a(ss_ram_din), .rdata_a(ss_ram_dout),
		.cs_b(st_cs), .rd_b(st_rd), .wr_b(st_wr), .mask_b(st_mask), .nwait_b(st_wait),
		.addr_b(st_addr), .wdata_b(st_din), .rdata_b(st_dout),
		.sd_cke(sd_cke), .sd_cs(sd_cs), .sd_ras(sd_ras), .sd_cas(sd_cas), .sd_we(sd_we),
		.sd_addr(sd_addr), .sd_ba(sd_ba), .sd_dqm(sd_dqm), .sd_data(sd_data),
		.ext_refresh(ext_refresh), .reg_sdram(reg_sdram)
	);

	sdram_model u_sdram(
		.clk(sdclk), .cke(sd_cke), .cs_n(sd_cs), .ras_n(sd_ras), .cas_n(sd_cas), .we_n(sd_we),
		.a(sd_addr), .ba(sd_ba), .dqm(sd_dqm), .dq(sd_data)
	);

	// contention on the SDRAM data bus
	int contention = 0;
	always @(posedge clk) if (u_hub.sd_oe && u_sdram.dq_oe) contention++;

	// Saturn refresh cycle: the SCU runs it as a bus cycle of its own (FC1 low,
	// no chip select) about every 15us, between accesses (see scu_refresh).
	// The pulse comes from the same edge detector as SSMaster.v (ssfc1_s*).
	logic SS_FC1 = 1, f0 = 1, f1 = 1, f2 = 1;
	always @(posedge clk) begin f0 <= SS_FC1; f1 <= f0; f2 <= f1; end
	always @* ext_refresh = (f2 == 1 && f1 == 0);
	realtime last_ext = -1e9;
	always @(posedge clk) if (ext_refresh) last_ext = $realtime;

	// ---------------- address maps ----------------
	// A (Saturn) test words w = 0..65535: bank 0, row {1, w[15:8], 0}, col w[7:0]
	function automatic logic [23:0] a_bus_addr(logic [15:0] w);
		return {2'b01, w[15:8], 5'b00000, w[7:0], 1'b0};
	endfunction
	function automatic int a_index(logic [15:0] w);
		return {2'b00, 1'b1, w[15:8], 1'b0, 2'b00, w[7:0]};
	endfunction
	function automatic logic [15:0] swap(logic [15:0] v);
		return {v[7:0], v[15:8]};
	endfunction

	logic [15:0] shadow [65536];
	initial for (int w = 0; w < 65536; w++) shadow[w] = swap(u_sdram.init_val(a_index(w[15:0])));

	// ---------------- statistics ----------------
	int n_rd = 0, n_wr = 0, n_err = 0, n_late = 0, lat_n = 0;
	real lat_max = 0, lat_sum = 0;
	logic        rd_active = 0, lat_done = 0;
	logic [15:0] rd_expect;
	realtime     rd_t0;
	real         budget;

	// error classification: what was going on when the read started
	bit busy_at_start, saw_start, st_busy_at_start;
	int e_busy = 0, e_nostart = 0, e_ref = 0, e_ref_ext = 0, e_st = 0, e_other = 0, e_stale = 0;
	logic [15:0] prev_smp = 16'hxxxx;
	always @(posedge clk) begin
		if (ss_rd_start && rd_active) begin
			saw_start = 1;
			busy_at_start = (u_hub.busa.state != 0);
			st_busy_at_start = (u_hub.state != 0 && u_hub.act_ab == 1) || (u_hub.cmd_req_b != 0);
		end
	end

	// why the last refresh was issued (only meaningful for the 'new' design)
	string   ref_why = "?", why_d = "?";
	realtime ref_t = -1e9;
	always @(posedge clk) begin
		if (u_hub._tsd.ref_ack) begin ref_t = $realtime; ref_why = why_d; end
`ifdef HAS_REF_DEBT
		// flags as the scheduler saw them on the clock that decided
		why_d = $sformatf("ext=%0d after_sat=%0d in_hit=%0d quiet=%0d force=%0d debt=%0d",
			u_hub._tsd.ext_req, u_hub._tsd.after_sat, u_hub._tsd.in_hit, u_hub._tsd.quiet, u_hub._tsd.ref_force, u_hub._tsd.ref_debt);
`endif
	end

	// time until the bus shows the right value (any read)
	always @(posedge clk) if (rd_active && !lat_done && SS_DATA === rd_expect) begin
		real l;
		l = $realtime - rd_t0;
		lat_done = 1; lat_n++; lat_sum += l;
		if (l > lat_max) lat_max = l;
	end

	// ---------------- SCU master ----------------
	task automatic scu_access(bit wr, logic [15:0] w, logic [15:0] wd);
		logic [15:0] smp;
		@(posedge sclk); #2;
		SS_ADDR = a_bus_addr(w);
		SS_CS0n = 0;
		if (wr) begin
			// The new write data reaches the FPGA WDATA_DELAY ns after the cycle starts; until then the
			// previous value stays on the bus (hardware 2026-11-24: a write latched ~40ns in kept a stale bit)
			scu_drv_en = 1; SS_WR0n = 0; SS_WR1n = 0;
			if (WDATA_DELAY > 0.0) fork begin #(WDATA_DELAY); scu_drv = wd; end join_none
			else scu_drv = wd;
		end
		else begin SS_RDn = 0; rd_expect = shadow[w]; rd_t0 = $realtime; lat_done = 0; saw_start = 0; busy_at_start = 0; st_busy_at_start = 0; rd_active = 1; end
		// The SCU samples SAMPLE_ADV ns before the end of the cycle (stands in for the delays the model doesn't
		// have: bus transceivers, board, SCU input setup, FPGA pin delays). Without external wait the end of the
		// cycle is fixed, so the sample time is set from the start of the cycle (any SAMPLE_ADV works).
		if (!EWT) fork
			begin #((NW + 1) * TSCU - 2.0 - SAMPLE_ADV); smp = SS_DATA; end
		join_none
		repeat (NW) @(posedge sclk);
		if (EWT) begin
			while (ss_ram_wait == 0) @(posedge sclk);
			#(TSCU - SAMPLE_ADV);
			smp = SS_DATA;
		end
		@(posedge sclk);
		#2;
		SS_CS0n = 1; SS_RDn = 1; SS_WR0n = 1; SS_WR1n = 1; scu_drv_en = 0;
		if (wr) begin
			n_wr++; shadow[w] = wd;
		end else begin
			n_rd++; rd_active = 0;
			if (!lat_done) n_late++;
			if (smp !== rd_expect) begin
				n_err++;
				if (smp === prev_smp) e_stale++;	// the value of the previous read: answered too late
				if (!saw_start)                                  e_nostart++;
				else if (busy_at_start)                          e_busy++;
				else if ($realtime - u_sdram.last_ref < 250.0) begin
					if ($realtime - last_ext < 400.0) e_ref_ext++; else e_ref++;
				end
				else if (st_busy_at_start)                       e_st++;
				else                                             e_other++;
				if (n_err <= 8) $display("%t READ ERROR w=%04h got %04h expected %04h | last refresh %.0fns before read start (%s)",
					$realtime, w, smp, rd_expect, rd_t0 - ref_t, ref_why);
			end
			prev_smp = smp;
		end
		if (RPC && !wr) @(posedge sclk);	// read precharge only (Sega 0x2330: A0RPC=1, A0WPC=0)
		if (WPC &&  wr) @(posedge sclk);
	endtask

	// SCU refresh cycle: AREF = 0x13 -> refresh enabled, 3 wait clocks
	task automatic scu_refresh();
		@(posedge sclk); #2;
		SS_FC1 = 0;
		repeat (4) @(posedge sclk);
		#2; SS_FC1 = 1;
		@(posedge sclk);
	endtask

	// ---------------- STM32 master ----------------
	int  st_ops = 0;
	real st_wait_max = 0, st_wait_sum = 0;
	task automatic st_access(bit wr, logic [25:0] a, logic [15:0] d);
		realtime t0;
		real tw;
		@(posedge clk);
		t0 = $realtime;
		st_cs <= 1; st_addr <= a; st_din <= d; st_mask <= 2'b00;
		if (wr) st_wr <= 1; else st_rd <= 1;
		@(posedge clk); st_wr <= 0; st_rd <= 0;
		@(posedge clk);
		while (st_wait == 0) @(posedge clk);
		@(posedge clk); st_cs <= 0;
		tw = $realtime - t0;
		st_ops++; st_wait_sum += tw;
		if (tw > st_wait_max) st_wait_max = tw;
	endtask

	initial begin : stm32
		int unsigned gap, run;
		logic [25:0] base;
		bit wr;
		wait (reset);
		wait (u_hub._tsd.m_state == 1);
		if (B_LOAD > 0) forever begin
			gap = (B_LOAD == 1) ? $urandom_range(400, 40) : $urandom_range(6, 0);
			repeat (gap) @(posedge clk);
			run = $urandom_range(16, 1);
			wr  = ($urandom % 4) != 0;			// mostly writes, like sector fills
			// B area: row[9] = 0, so it never overlaps the Saturn test words
			base = {1'b1, 2'b00, 1'b0, 9'($urandom), 2'($urandom), 10'($urandom), 1'b0};
			for (int i = 0; i < run; i++) st_access(wr, base + 26'(i * 2), 16'($urandom));
		end
	end

	// ---------------- main ----------------
	initial begin : main
		logic [15:0] w;
		int unsigned r;
		realtime next_ref;
		void'($urandom(SEED));
		budget = (NW + 1) * TSCU;
		wait (reset);
		wait (u_hub._tsd.m_state == 1);		// SDRAM power-on sequence done
		#1000;
		w = 16'($urandom);
		next_ref = $realtime + 15000.0;
		while ($realtime < 1000.0 * SIM_US + 210000.0) begin
			if ($realtime >= next_ref) begin scu_refresh(); next_ref += 15000.0; end
			if (TRAFFIC == 1) begin
				// Like RAMCART-Stress test 5: 32-bit write A, read B, read A (each longword = 2 bus cycles,
				// high halfword first), no idle clocks
				logic [15:0] a, b, hi, lo;
				a = {16'($urandom) & 16'hFFFE};
				b = {16'($urandom) & 16'hFFFE};
				hi = 16'($urandom); lo = 16'($urandom);
				scu_access(1, a, hi); scu_access(1, a + 1, lo);
				scu_access(0, b, 16'h0); scu_access(0, b + 1, 16'h0);
				scu_access(0, a, 16'h0); scu_access(0, a + 1, 16'h0);
			end else if (TRAFFIC == 2) begin
				// Like RAMCART-Stress test 3 (verify): sequential 32-bit reads
				scu_access(0, w, 16'h0); scu_access(0, w + 1, 16'h0);
				w = w + 2;
			end else begin
			r = $urandom % 100;
			if (r < 55)      w = w + 1;			// sequential
			else if (r < 70) ;				// same word again
			else             w = 16'($urandom);	// random
			if (($urandom % 100) < 30) scu_access(1, w, 16'($urandom));
			else                       scu_access(0, w, 16'h0);
			repeat ($urandom % 3) @(posedge sclk);	// 0-2 idle clocks
			end
			if (($urandom % 100) < SAT_IDLE_PCT) #($urandom_range(3000, 500));	// Saturn busy elsewhere
		end
		#2000;
		$display("STM32 %s | accesses %0d | wait avg %.0f max %.0f ns", NAME, st_ops,
			(st_ops ? st_wait_sum / st_ops : 0.0), st_wait_max);
		$display("STALE %s | errors that returned the previous read's value: %0d of %0d", NAME, e_stale, n_err);
		$display("CAUSES %s | read start missed %0d | cachebus still busy %0d | internal refresh %0d | Saturn-refresh refresh %0d | STM32 in progress %0d | other (too slow) %0d",
			NAME, e_nostart, e_busy, e_ref, e_ref_ext, e_st, e_other);
		$display("RESULT %s NW=%0d EWT=%0d B_LOAD=%0d | reads %0d writes %0d | READ ERRORS %0d | late %0d | read latency avg %.1f max %.1f ns (SCU samples at %.1f) | SDRAM errors %0d contention %0d | refs %0d max gap %.2f us | sdram rd %0d wr %0d",
			NAME, NW, EWT, B_LOAD, n_rd, n_wr, n_err, n_late, (lat_n ? lat_sum / lat_n : 0.0), lat_max, budget,
			u_sdram.errors, contention, u_sdram.refs, u_sdram.max_ref_gap / 1000.0, u_sdram.reads, u_sdram.writes);
		$finish;
	end
endmodule
