/* -*- c -*-  (not really, but good for syntax highlighting) */

%extend struct _CollectionIter {

  %pythoncode %{
    def __iter__(self):
        return self

    def __next__(self):
        next = self.next()
        if next is None:
            raise StopIteration()
        return next
  %}
}


%extend struct _DLiteCollection {

  %pythoncode %{
    def __repr__(self):
        return "Collection(%r)" % (self.uri if self.uri else self.uuid)

    def __str__(self):
        return str(self.asinstance())

    def __iter__(self):
        return self.get_iter()

    def __getitem__(self, label):
        return self.get(label)

    meta = property(get_meta, doc='Reference to metadata of this collection.')

    def asdict(self):
        """Returns a dict representation of self."""
        return self.asinstance().asdict()

    def asjson(self, **kwargs):
        """Returns a JSON-representation of self. Arguments are passed to
        json.dumps()."""
        return self.asinstance().asjson()

    def relations(self, s=None, p=None, o=None):
        """Returns a generator over all relations matching the given
        values of `s`, `p` and `o`."""
        itr = self.get_iter()
        while itr.poll():
            yield itr.find(s, p, o)

  %}
}
