class Vector2Array
{
    private array<double> x, y;

    uint Size()
    {
        if(x.Size() == y.Size() )
            return x.Size();
        
        else
            ThrowAbortException("Vector2 array: size of component arrays mismatch");
        
        return 0;
    }

    void Copy(Vector_2_array vec_arr)
    {
        x.Copy(vec_arr.x);
        y.Copy(vec_arr.y);
    }

    void Move(Vector_2_array vec_arr)
    {
        x.Move(vec_arr.x);
        y.Move(vec_arr.y);
    }

    void Append(Vector_2_array vec_arr)
    {
        x.Append(vec_arr.x);
        y.Append(vec_arr.y);
    }

    uint Find(vector2 vec) const
    {
        int index = x.Find(vec.x);
        if(index == x.Size() )
            return index;
        
        if(index < y.Size() )
        {
            if(y[index])
            {
                if(y[index] != vec.y)
                    return x.Size();
            
                else
                    return index;
            }
        }
        else
            ThrowAbortException("Vector2 array: size of component arrays mismatch");

        return x.Size();
    }

    uint Push(vector2 vec)
    {
        x.Push(vec.x);
        y.Push(vec.y);

        if(x.Size() != y.Size() )
            ThrowAbortException("Vector2 array: size of component arrays mismatch");
        
        return x.Size();
    }

    bool Pop()
    {
        bool x_res = x.Pop();
        bool y_res = y.Pop();

        if(x_res == y_res)
            return x_res;
        
        else
            ThrowAbortException("Vector3 array: return values dont match");

        return false;
    }

    void Delete(int index, int deletecount = 1)
    {
        if(index > -1 && index < x.Size() && index < y.Size() )
        {
            x.Delete(index, deletecount);
            y.Delete(index, deletecount);
        }
        else
            ThrowAbortException("Vector2 array: index outside of array bounds");
    }

    void Insert(uint index, vector2 vec)
    {
        x.insert(index, vec.x);
        y.insert(index, vec.y);
    }

    void ShrinkToFit()
    {
        x.ShrinkToFit();
        y.ShrinkToFit();
    }

    void Grow(uint count)
    {
        x.Grow(count);
        y.Grow(count);
    }

    void Resize(int index)
    {
        x.Resize(index);
        y.Resize(index);
    }

    uint Reserve (uint amount)
    {
        uint x_res = x.Reserve(amount);
        uint y_res = y.Reserve(amount);
    
        if(x_res == y_res)
            return x_res;

        else
            ThrowAbortException("Vector2 array: return values dont match");
        
        return 0;    
    }

    uint Max()
    {
        uint x_res = x.Max();
        uint y_res = y.Max();
    
        if(x_res == y_res)
            return x_res;
        
        else
            ThrowAbortException("Vector2 array: allocated entries of component arrays mismatch");
    
        return 0;
    }

    void Clear()
    {
        x.Clear();
        y.Clear();
    }

    vector2 Get(int index) const
    {
        if(index > -1 && index < x.Size() && index < y.Size() )
            return (x[index], y[index]);
        else
            ThrowAbortException("Vector2 array: index outside of array bound");

        return (0, 0);
    }

}




class Vector3Array
{
    private array<double> x, y, z;

    uint Size()
    {
        if(x.Size() == y.Size() == z.Size() )
            return x.Size();
        else
            ThrowAbortException("Vector3 array: size of component arrays mismatch");
    
        return 0;
    }

    void Copy(Vector_3_array vec_arr)
    {
        x.Copy(vec_arr.x);
        y.Copy(vec_arr.y);
        z.Copy(vec_arr.z);
    }

    void Move(Vector_3_array vec_arr)
    {
        x.Move(vec_arr.x);
        y.Move(vec_arr.y);
        z.Move(vec_arr.z);
    }

    void Append(Vector_3_array vec_arr)
    {
        x.Append(vec_arr.x);
        y.Append(vec_arr.y);
        z.Append(vec_arr.z);
    }

    uint Find(vector3 vec) const
    {
        int index = x.Find(vec.x);
        if(index == x.Size() )
            return index;
        
        if(index < y.Size() && index < z.Size() )
        {
            if(y[index] && z[index])
            {
                if(y[index] != vec.y)
                    return x.Size();

                if(z[index] != vec.z)
                    return x.Size();

                return index;
            }
        }
        else
            ThrowAbortException("Vector3 array: size of component arrays mismatch");

        return x.Size();
    }

    uint Push(vector3 vec)
    {
        x.Push(vec.x);
        y.Push(vec.y);
        z.Push(vec.z);

        if(x.Size() != y.Size() || x.Size() != z.Size() || y.Size() != z.Size() )
            ThrowAbortException("Vector3 array: size of component arrays mismatch");
        
        return x.Size();
    }

    bool Pop()
    {
        bool x_res = x.Pop();
        bool y_res = y.Pop();
        bool z_res = z.Pop();

        if(x_res == y_res == z_res)
            return x_res;

        else
            ThrowAbortException("Vector3 array: size of component arrays mismatch");

        return false;
    }

    void Delete(int index, int deletecount = 1)
    {
        if(index > -1 && index < x.Size() && index < y.Size() )
        {
            x.Delete(index, deletecount);
            y.Delete(index, deletecount);
        }
        else
            ThrowAbortException("Vector3 array: index outside of array bounds");
    }

    void Insert(uint index, vector3 vec)
    {
        x.Insert(index, vec.x);
        y.Insert(index, vec.y);
        z.Insert(index, vec.z);
    }

    void ShrinkToFit()
    {
        x.ShrinkToFit();
        y.ShrinkToFit();
        z.ShrinkToFit();
    }

    void Grow(uint count)
    {
        x.Grow(count);
        y.Grow(count);
        z.Grow(count);
    }

    void Resize(int index)
    {
        x.Resize(index);
        y.Resize(index);
        z.Resize(index);
    }

    uint Reserve (uint amount)
    {
        uint x_res = x.Reserve(amount);
        uint y_res = y.Reserve(amount);
        uint z_res = z.Reserve(amount);
    
        if(x_res == y_res == z_res)
            return x_res;

        else
            ThrowAbortException("Vector3 array: return values dont match");
        
        return 0;    
    }

    uint Max()
    {
        uint x_res = x.Max();
        uint y_res = y.Max();
        uint z_res = z.Max();
    
        if(x_res == y_res == z_res)
            return x_res;
        
        else
            ThrowAbortException("Vector3 array: allocated entries of component arrays mismatch");
    
        return 0;
    }

    void Clear()
    {
        x.Clear();
        y.Clear();
        z.Clear();
    }

    vector3 Get(int index) const
    {
        if(index > -1 && index < x.Size() && index < y.Size() && index < z.Size() )
            return (x[index], y[index], z[index]);
        else
            ThrowAbortException("Vector3 array: index outside of array bound");

        return (0, 0, 0);
    }

    
}
