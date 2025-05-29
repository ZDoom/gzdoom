class Haptics
{
    native static void RumbleDirect(int tic_count, float high_frequency, float low_frequency, float left_trigger, float right_trigger);
    native static void Rumble(Name identifier);
    native static void RumbleOr(Name identifier, Name fallback = '');
}
