
template <typename DataT>
class DataChannel {
 public:
    bool connect();
    bool send(const DataT &data);
    bool close();
    bool is_connected();
};