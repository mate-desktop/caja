#!/usr/bin/perl
=pod

    update-authors.pl is part of Caja.

    Caja is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Caja is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Caja.  If not, see <https://www.gnu.org/licenses/>.

=cut
use strict;
use warnings;

sub ReplaceAuthors {
  my @authors = @_;
  $_ eq 'alexandervdm <alexandervdm%gliese.me>' and $_ = 'Alexander van der Meij <alexandervdm%gliese.me>' for @authors;
  $_ eq 'bkerlin <me%brentkerlin.com>' and $_ = 'Brent Kerlin <me%brentkerlin.com>' for @authors;
  $_ eq 'bl0ckeduser <bl0ckedusersoft%gmail.com>' and $_ = 'Gabriel Cormier-Affleck <bl0ckedusersoft%gmail.com>' for @authors;
  $_ eq 'chat-to-me%raveit.de <Nice&Gently>' and $_ = 'Wolfgang Ulbrich <mate%raveit.de>' for @authors;
  $_ eq 'chen donghai <chen.donghai%zte.com.cn>' and $_ = 'Chen Donghai <chen.donghai%zte.com.cn>' for @authors;
  $_ eq 'Clément Masci <>' and $_ = 'Clément Masci' for @authors;
  $_ eq 'emanuele-f <black.silver%hotmail.it>' and $_ = 'Emanuele Faranda <black.silver%hotmail.it>' for @authors;
  $_ eq 'ericcurtin <ericcurtin17%gmail.com>' and $_ = 'Eric Curtin <ericcurtin17%gmail.com>' for @authors;
  $_ eq 'hekel <hekel%archlinux.info>' and $_ = 'Adam Erdman <hekel%archlinux.info>' for @authors;
  $_ eq 'infirit <infirit%gmail.com>' and $_ = 'Sander Sweers <infirit%gmail.com>' for @authors;
  $_ eq 'leigh123linux <leigh123linux%googlemail.com>' and $_ = 'Leigh Scott <leigh123linux%googlemail.com>' for @authors;
  $_ eq 'Martin Wimpress <code%flexion.org>' and $_ = 'Martin Wimpress <martin%mate-desktop.org>' for @authors;
  $_ eq 'Monsta <monsta%inbox.ru>' and $_ = 'Vlad Orlov <monsta%inbox.ru>' for @authors;
  $_ eq 'monsta <monsta%inbox.ru>' and $_ = 'Vlad Orlov <monsta%inbox.ru>' for @authors;
  $_ eq 'OBATA Akio <obache%users.noreply.github.com>' and $_ = 'Obata Akio <obache%users.noreply.github.com>' for @authors;
  $_ eq 'raveit65 <chat-to-me%raveit.de>' and $_ = 'Wolfgang Ulbrich <mate%raveit.de>' for @authors;
  $_ eq 'raveit65 <mate%raveit.de>' and $_ = 'Wolfgang Ulbrich <mate%raveit.de>' for @authors;
  $_ eq 'rbuj <robert.buj%gmail.com>' and $_ = 'Robert Buj <robert.buj%gmail.com>' for @authors;
  $_ eq 'romovs <romovs%gmail.com>' and $_ = 'Roman Ovseitsev <romovs%gmail.com>' for @authors;
  $_ eq 'tarakbumba <tarakbumba%gmail.com>' and $_ = 'Atilla ÖNTAŞ <tarakbumba%gmail.com>' for @authors;
  $_ eq 'Victor Kareh <vkareh%vkareh.net>' and $_ = 'Victor Kareh <vkareh%redhat.com>' for @authors;
  $_ eq 'Wolfgang Ulbrich <chat-to-me%raveit.de>' and $_ = 'Wolfgang Ulbrich <mate%raveit.de>' for @authors;
  $_ eq 'yetist <xiaotian.wu%i-soft.com.cn>' and $_ = 'Wu Xiaotian <yetist%gmail.com>' for @authors;
  return @authors;
}

sub GetCurrentAuthors {
  my @authors;
  open(FILE,"src/caja.about") or die "Can't open src/caja.about";
  while (<FILE>) {
    if (/^Authors=*(.+)$/) {
      @authors=split(";",$1);
    }
  }
  close FILE;
  return ReplaceAuthors(@authors);
}

sub GetNewAuthors {
  my @authors = `git log --pretty="%an <%ae>" --since "2012-01-01" -- . "_.h" "_.c" | sort | uniq | sed 's/@/%/g' | sed '/^mate-i18n.*/d'`;
  chomp @authors;
  return ReplaceAuthors(@authors);
}

my @A = GetCurrentAuthors;
my @B = GetNewAuthors;
my @merged = sort { $a cmp $b } keys %{{map {($_ => 1)} (@A, @B)}};
print join(';',@merged) . ';';
