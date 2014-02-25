enum cell_flags_t{
            Cell_Fixed      =0x1,
            Cell_Insulator  =0x2
        };

__kernel void kernel_xy(
    float inner, float outer, 
    __global const float *world_state, 
    __global const uint *world_properties,
    __global float *buffer )
{
    uint x=get_global_id(0);
    uint y=get_global_id(1);
    uint w=get_global_size(0);
    unsigned index = y * w + x;

    uint curr_props= world_properties[index];

    if ((world_properties[index] & Cell_Fixed) || (world_properties[index] & Cell_Insulator))
    {
        // Do nothing, this cell never changes (e.g. a boundary, or an interior fixed-value heat-source)
        buffer[index] = world_state[index];
    }
    else
    {
        float contrib = inner;
        float acc = inner * world_state[index];

        // Cell above
        if (curr_props & 4)
        {
            contrib += outer;
            acc += outer * world_state[index - w];
        }

        // Cell below
        if (curr_props & 8)
        {
            contrib += outer;
            acc += outer * world_state[index + w];
        }

        // Cell left
        if (curr_props & 16)
        {
            contrib += outer;
            acc += outer * world_state[index - 1];
        }

        // Cell right
        if (curr_props & 32)
        {
            contrib += outer;
            acc += outer * world_state[index + 1];
        }

        // Scale the accumulate value by the number of places contributing to it
        float res = acc / contrib;
        // Then clamp to the range [0,1]
        res = min(1.0f, max(0.0f, res));
        buffer[index] = res;

    }
}