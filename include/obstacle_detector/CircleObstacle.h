// Generated by gencpp from file obstacle_detector/CircleObstacle.msg
// DO NOT EDIT!


#ifndef OBSTACLE_DETECTOR_MESSAGE_CIRCLEOBSTACLE_H
#define OBSTACLE_DETECTOR_MESSAGE_CIRCLEOBSTACLE_H


#include <string>
#include <vector>
#include <map>

#include <ros/types.h>
#include <ros/serialization.h>
#include <ros/builtin_message_traits.h>
#include <ros/message_operations.h>

#include <geometry_msgs/Point.h>
#include <geometry_msgs/Vector3.h>

namespace obstacle_detector
{
template <class ContainerAllocator>
struct CircleObstacle_
{
  typedef CircleObstacle_<ContainerAllocator> Type;

  CircleObstacle_()
    : center()
    , velocity()
    , radius(0.0)
    , obstacle_id()
    , tracked(false)  {
    }
  CircleObstacle_(const ContainerAllocator& _alloc)
    : center(_alloc)
    , velocity(_alloc)
    , radius(0.0)
    , obstacle_id(_alloc)
    , tracked(false)  {
  (void)_alloc;
    }



   typedef  ::geometry_msgs::Point_<ContainerAllocator>  _center_type;
  _center_type center;

   typedef  ::geometry_msgs::Vector3_<ContainerAllocator>  _velocity_type;
  _velocity_type velocity;

   typedef double _radius_type;
  _radius_type radius;

   typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other >  _obstacle_id_type;
  _obstacle_id_type obstacle_id;

   typedef uint8_t _tracked_type;
  _tracked_type tracked;




  typedef boost::shared_ptr< ::obstacle_detector::CircleObstacle_<ContainerAllocator> > Ptr;
  typedef boost::shared_ptr< ::obstacle_detector::CircleObstacle_<ContainerAllocator> const> ConstPtr;

}; // struct CircleObstacle_

typedef ::obstacle_detector::CircleObstacle_<std::allocator<void> > CircleObstacle;

typedef boost::shared_ptr< ::obstacle_detector::CircleObstacle > CircleObstaclePtr;
typedef boost::shared_ptr< ::obstacle_detector::CircleObstacle const> CircleObstacleConstPtr;

// constants requiring out of line definition



template<typename ContainerAllocator>
std::ostream& operator<<(std::ostream& s, const ::obstacle_detector::CircleObstacle_<ContainerAllocator> & v)
{
ros::message_operations::Printer< ::obstacle_detector::CircleObstacle_<ContainerAllocator> >::stream(s, "", v);
return s;
}

} // namespace obstacle_detector

namespace ros
{
namespace message_traits
{



// BOOLTRAITS {'IsFixedSize': False, 'IsMessage': True, 'HasHeader': False}
// {'geometry_msgs': ['/root/cross-compiling/ubuntu-rootfs/opt/ros/indigo/share/geometry_msgs/cmake/../msg'], 'std_msgs': ['/root/cross-compiling/ubuntu-rootfs/opt/ros/indigo/share/std_msgs/cmake/../msg'], 'obstacle_detector': ['/root/gmapping/src/obstacle_detector/msg']}

// !!!!!!!!!!! ['__class__', '__delattr__', '__dict__', '__doc__', '__eq__', '__format__', '__getattribute__', '__hash__', '__init__', '__module__', '__ne__', '__new__', '__reduce__', '__reduce_ex__', '__repr__', '__setattr__', '__sizeof__', '__str__', '__subclasshook__', '__weakref__', '_parsed_fields', 'constants', 'fields', 'full_name', 'has_header', 'header_present', 'names', 'package', 'parsed_fields', 'short_name', 'text', 'types']




template <class ContainerAllocator>
struct IsFixedSize< ::obstacle_detector::CircleObstacle_<ContainerAllocator> >
  : FalseType
  { };

template <class ContainerAllocator>
struct IsFixedSize< ::obstacle_detector::CircleObstacle_<ContainerAllocator> const>
  : FalseType
  { };

template <class ContainerAllocator>
struct IsMessage< ::obstacle_detector::CircleObstacle_<ContainerAllocator> >
  : TrueType
  { };

template <class ContainerAllocator>
struct IsMessage< ::obstacle_detector::CircleObstacle_<ContainerAllocator> const>
  : TrueType
  { };

template <class ContainerAllocator>
struct HasHeader< ::obstacle_detector::CircleObstacle_<ContainerAllocator> >
  : FalseType
  { };

template <class ContainerAllocator>
struct HasHeader< ::obstacle_detector::CircleObstacle_<ContainerAllocator> const>
  : FalseType
  { };


template<class ContainerAllocator>
struct MD5Sum< ::obstacle_detector::CircleObstacle_<ContainerAllocator> >
{
  static const char* value()
  {
    return "d6718cc6abcb5fe088cbbd76d2e742d1";
  }

  static const char* value(const ::obstacle_detector::CircleObstacle_<ContainerAllocator>&) { return value(); }
  static const uint64_t static_value1 = 0xd6718cc6abcb5fe0ULL;
  static const uint64_t static_value2 = 0x88cbbd76d2e742d1ULL;
};

template<class ContainerAllocator>
struct DataType< ::obstacle_detector::CircleObstacle_<ContainerAllocator> >
{
  static const char* value()
  {
    return "obstacle_detector/CircleObstacle";
  }

  static const char* value(const ::obstacle_detector::CircleObstacle_<ContainerAllocator>&) { return value(); }
};

template<class ContainerAllocator>
struct Definition< ::obstacle_detector::CircleObstacle_<ContainerAllocator> >
{
  static const char* value()
  {
    return "geometry_msgs/Point center      # Central point [m]\n\
geometry_msgs/Vector3 velocity  # Linear velocity [m/s]\n\
float64 radius                  # Radius [m]\n\
\n\
string obstacle_id              # ID of the obstacle\n\
bool tracked                    # If the obstacle is tracked\n\
\n\
================================================================================\n\
MSG: geometry_msgs/Point\n\
# This contains the position of a point in free space\n\
float64 x\n\
float64 y\n\
float64 z\n\
\n\
================================================================================\n\
MSG: geometry_msgs/Vector3\n\
# This represents a vector in free space. \n\
# It is only meant to represent a direction. Therefore, it does not\n\
# make sense to apply a translation to it (e.g., when applying a \n\
# generic rigid transformation to a Vector3, tf2 will only apply the\n\
# rotation). If you want your data to be translatable too, use the\n\
# geometry_msgs/Point message instead.\n\
\n\
float64 x\n\
float64 y\n\
float64 z\n\
";
  }

  static const char* value(const ::obstacle_detector::CircleObstacle_<ContainerAllocator>&) { return value(); }
};

} // namespace message_traits
} // namespace ros

namespace ros
{
namespace serialization
{

  template<class ContainerAllocator> struct Serializer< ::obstacle_detector::CircleObstacle_<ContainerAllocator> >
  {
    template<typename Stream, typename T> inline static void allInOne(Stream& stream, T m)
    {
      stream.next(m.center);
      stream.next(m.velocity);
      stream.next(m.radius);
      stream.next(m.obstacle_id);
      stream.next(m.tracked);
    }

    ROS_DECLARE_ALLINONE_SERIALIZER
  }; // struct CircleObstacle_

} // namespace serialization
} // namespace ros

namespace ros
{
namespace message_operations
{

template<class ContainerAllocator>
struct Printer< ::obstacle_detector::CircleObstacle_<ContainerAllocator> >
{
  template<typename Stream> static void stream(Stream& s, const std::string& indent, const ::obstacle_detector::CircleObstacle_<ContainerAllocator>& v)
  {
    s << indent << "center: ";
    s << std::endl;
    Printer< ::geometry_msgs::Point_<ContainerAllocator> >::stream(s, indent + "  ", v.center);
    s << indent << "velocity: ";
    s << std::endl;
    Printer< ::geometry_msgs::Vector3_<ContainerAllocator> >::stream(s, indent + "  ", v.velocity);
    s << indent << "radius: ";
    Printer<double>::stream(s, indent + "  ", v.radius);
    s << indent << "obstacle_id: ";
    Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other > >::stream(s, indent + "  ", v.obstacle_id);
    s << indent << "tracked: ";
    Printer<uint8_t>::stream(s, indent + "  ", v.tracked);
  }
};

} // namespace message_operations
} // namespace ros

#endif // OBSTACLE_DETECTOR_MESSAGE_CIRCLEOBSTACLE_H
