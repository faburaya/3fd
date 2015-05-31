#ifndef BASE_H
#define BASE_H

namespace _3fd
{
	////////////////////////////////
	// Utility Base Classes
	////////////////////////////////

	/// <summary>
	/// If a class derives NotCopiable, it cannot be copied.
	/// If you try to do that, an error is reported during the compilation.
	/// </summary>
	class NotCopiable
	{
	private:

        NotCopiable(const NotCopiable &) {}

	public:

        NotCopiable() {}
	};

#	define notcopiable	public ::_3fd::NotCopiable
}

#endif // end of header guard
