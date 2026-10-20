#include "IRLutAxisIndex.h"
#include <iostream>
#include <cstdlib>

void expect(bool ok, const char* label)
{
	if (!ok) { std::cerr << "FAIL " << label << std::endl; std::exit(1); }
}
int main()
{
	IRLutAxisIndex index;
	index.add({.0001}, 0); index.add({.001}, 1);
	std::vector<IRLutAxisIndex::WeightedRow> rows; IRLutAxisIndex::Error error;
	expect(index.query({.0005}, {.001}, rows, error), "submetre interval available");
	expect(rows.size()==2 && rows[0].weight>0 && rows[1].weight>0, "positive range not snapped to endpoint");
	expect(std::abs(rows[1].weight-4.0/9.0)<1e-12, "submetre interpolation weight");
	expect(!index.query({0.0}, {.001}, rows, error), "zero not silently replaced by first positive range");
	IRLutAxisIndex missing;
	missing.add({3,1},0); missing.add({20,2},1);
	expect(!missing.query({10,1.5},{1e-8,1e-8},rows,error), "missing cell rejected");
	index.add({.001},2);
	expect(!index.query({.001},{1e-8},rows,error), "duplicate node rejected");
	std::cout<<"PASS sub-metre interpolation, no zero snapping, missing cells and duplicates"<<std::endl;
}
