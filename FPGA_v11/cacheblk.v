
///////////////////////////////////////////////////////
// Module: Cache Block manager                       //
///////////////////////////////////////////////////////

module cacheblk(
	reset, clk,
	cache_addr, cache_data1d, cache_valid, data_valid, cdata,
	cache_update_a, umask_a, uaddr_a, udata_a,
	cache_update_b, umask_b, uaddr_b, udata_b,
	cache_invalid, iaddr
);

///////////////////////////////////////////////////////
// Pins                                              //
///////////////////////////////////////////////////////

	// system
	input reset;
	input clk;

	// cache interface
	output[25:3] cache_addr;
	output[63:0] cache_data1d;
	output[ 3:0] cache_valid;
	input data_valid;
	input[15:0] cdata;

	// cache update
	input cache_update_a;
	input[ 1:0] umask_a;
	input[25:0] uaddr_a;
	input[15:0] udata_a;

	input cache_update_b;
	input[ 1:0] umask_b;
	input[25:0] uaddr_b;
	input[15:0] udata_b;

	// cache incalid
	input cache_invalid;
	input[25:0] iaddr;


///////////////////////////////////////////////////////
// cache block manager                               //
///////////////////////////////////////////////////////

	reg[15:0] cache_data[3:0];
	reg[25:3] cache_addr;
	reg[ 3:0] cache_valid;
	reg[ 1:0] dcnt;
	
	assign cache_data1d = {cache_data[3],cache_data[2],cache_data[1],cache_data[0]};


	// cache_data
	always @(posedge clk)
	begin
		if(cache_update_a && uaddr_a[25:3]==cache_addr) begin
			if(umask_a[1]==0) cache_data[uaddr_a[2:1]][15:8] <= udata_a[15:8];
			if(umask_a[0]==0) cache_data[uaddr_a[2:1]][ 7:0] <= udata_a[ 7:0];
		end else if(cache_update_b && uaddr_b[25:3]==cache_addr) begin
			if(umask_b[1]==0) cache_data[uaddr_b[2:1]][15:8] <= udata_b[15:8];
			if(umask_b[0]==0) cache_data[uaddr_b[2:1]][ 7:0] <= udata_b[ 7:0];
		end else if(data_valid) begin
			// 是否已经被cache_update填充?
			if(cache_valid[dcnt]==0) begin
				cache_data[dcnt] <= cdata;
			end
		end
	end

	// cache_addr
	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			cache_addr <= 23'h555555;
		end else if(cache_invalid)begin
			cache_addr <= iaddr[25:3];
		end
	end

	// cache_valid
	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			cache_valid <= 0;
		end else if(cache_invalid)begin
			cache_valid <= 0;
		end else if(data_valid) begin
			cache_valid[dcnt] <= 1'b1;
		end
	end

	// dcnt
	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			dcnt <= 0;
		end else if(cache_invalid)begin
			dcnt <= 0;
		end else if(data_valid) begin
			dcnt <= dcnt+1'b1;
		end
	end


endmodule

