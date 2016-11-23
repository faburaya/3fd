const MATH_INPUT_ARRAY_SIZE  = 32;

struct SumInput {
    double parcels<MATH_INPUT_ARRAY_SIZE>;
};

struct MultiplyInput {
    double factors<MATH_INPUT_ARRAY_SIZE>;
};

typedef struct SumInput SumInput;
typedef struct MultiplyInput MultiplyInput;

program ACME_TESTING_PROGRAM {
    version ACME_TESTING_VERSION {
        double SUM(SumInput input) = 1;
        double MULTIPLY(MultiplyInput input) = 2;
        string TOUPPER(string text) = 3;
    } = 1;
} = 0x532d;

